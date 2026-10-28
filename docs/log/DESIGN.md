# Logging: proposed complete public contract

28 October 2026. Baseline **`f9ac49f1558f326129948bc06c829263448b7714`**.
**Proposal for Golden Wrench and Boss; no implementation authorized.**
[Source findings and ownership](RESEARCH.md) · [Engineering and acceptance](ROADMAP.md).

## Public surface

One `use log` module. All declarations below are proposed signatures, not existing APIs. Ordinary language syntax is supported by baseline modules/examples; these logging examples have not been compiled because the module does not exist yet.

### Types and configuration

| Type | Contract |
| --- | --- |
| `level` | Payload-free cases `trace`, `debug`, `info`, `warn`, `error`, `off`. Increasing threshold order; do not depend on numeric enum ordinals. `off` is a filter setting, not a record severity. |
| `console_sink` | `none`, `stdout`, `stderr`; borrowed existing endpoints. |
| `existing_files` | `reject` or `adopt`, with the filesystem authority requirements below. |
| `config` | Independent value struct with the following fields; no callbacks, mutable collection settings or native handles. |
| `logger` | Managed, explicitly owned logger. Private construction; use `open`/`open_async`. Assignments share one identity and lifecycle. |
| `failure_kind` | `invalid_config`, `invalid_level`, `record_too_large`, `busy`, `closed`, `faulted`, `clock`, `console_io`, `file_io`, `rotation_io`, `cleanup_io`. |
| `failure` | Managed error extending aliased `error.error`, with typed causes and partial progress below. The name preserves the desired `log.error()` function. |

Proposed `config` fields:

| Field and type | Default | Meaning |
| --- | --- | --- |
| `minimum: level` | `level.info` | Inclusive threshold; off disables all records. |
| `name: string` | `""` | Optional category/application label, always quoted in the record; no hierarchy or lookup registry. |
| `console: console_sink` | `console_sink.stderr` | One console endpoint, or none. |
| `timestamps: bool` | `true` | Fixed UTC timestamp prefix; no local timezone or custom timestamp language. |
| `file_path: string` | `""` | Empty disables file output; otherwise a host filesystem path, not a VFS/module import. |
| `rotation_bytes: i64` | `8388608` | Positive hard bound for each managed file, including complete record framing. No implicit unbounded-file mode. |
| `backups: i32` | `3` | 0–32 retained backup slots; zero retains only the bounded active file. This is an operational policy, not a language collection limit. |
| `max_record_bytes: i64` | `65536` | Positive byte budget for the fully encoded record. No truncation. |
| `existing: existing_files` | `existing_files.reject` | Fresh exclusive file set by default; adoption is an explicit authority decision. |

Validate enum choices, positive sizes, backup range, representable `(backups + 1) * rotation_bytes`, and enough record space for the fixed framing and escaped name. Let E(s) be escaped interior UTF-8 bytes and L the severity-label length (4 or 5). Record size is `32 + L + E(name) + E(message)` with timestamps, or `7 + L + E(name) + E(message)` without. Reserve at least `E(name)+37` or `E(name)+12`, respectively, so an empty message fits at every severity. Use checked arithmetic, not an unchecked worst-case expansion multiplication. With a file sink, `max_record_bytes <= rotation_bytes`. At least one sink is required unless `minimum` is off. Validate before filesystem mutation. Settings are copied; later edits to the caller's value do not reconfigure a logger.

### Functions and methods

The return spelling `R` in this table means exactly `result<bool, failure>`, not a new type alias or language feature.

| Module function | Instance receiver method | Result / purpose |
| --- | --- | --- |
| `open(options: config = config())` | — | `result<logger,failure>`; acquire an independent logger. Does not change the default. |
| `async open_async(options: config = config())` | — | Same result; use existing async filesystem acquisition/validation. |
| `configure(options: config)` | `configure(options: config)` | `R`; complete explicit reconfiguration, with nontransactional I/O semantics below. |
| `async configure_async(options: config)` | `async configure_async(options: config)` | `R`; awaitable file lifecycle. |
| `enabled(severity: level)` | `enabled(severity: level)` | `bool`; filter predicate only, no clock, formatting, output or writable-sink guarantee. Off returns false. |
| `write(severity: level, message: string)` | `write(severity: level, message: string)` | `R`; canonical synchronous emission. |
| `trace(message: string)` | `trace(message: string)` | `R`; forwards to write with trace. |
| `debug(message: string)` | `debug(message: string)` | `R`; forwards with debug. |
| `info(message: string)` | `info(message: string)` | `R`; forwards with info. |
| `warn(message: string)` | `warn(message: string)` | `R`; forwards with warn. |
| `error(message: string)` | `error(message: string)` | `R`; forwards with error, without exiting the program. |
| `write_lazy(severity: level, make_message: func() -> string)` | same | `R`; synchronous temporary builder; never saved. |
| `async write_async(severity: level, message: string)` | same | `R`; explicit awaitable file emission; no callback parameter. |
| `flush()` | `flush()` | `R`; drain console and request file synchronization. |
| `async flush_async()` | `async flush_async()` | `R`; awaitable file synchronization. |
| `shutdown()` | `close()` | `R`; flush and release owned file, never close borrowed streams. |
| `async shutdown_async()` | `async close_async()` | `R`; awaitable file lifecycle. |
| — | `deinit()` | No result; synchronous deterministic fallback for `with`/explicit cleanup. |

No overloads, variadic formatting arguments, five parallel lazy/async severity families, fatal/panic severity, custom formatter/sink registry, or public native resources. Error severity does not imply termination or a per-record fsync. The generic `write`/`write_lazy`/`write_async` methods supply every severity without multiplying aliases.

For emission, `Ok(false)` means filtered and no log-owned rendering/clock/output occurred. `Ok(true)` means the complete record was accepted by every configured sink. It does not imply power-loss durability. `Err` means failure, possibly after partial or complete output to one sink. Passing off to write is `invalid_level`; invalid severity and closed-state checks precede filtering. A valid filtered call can return false without waiting for another active emission; admitted same-instance work returns busy when reserved.

For acquisition/configuration/flush/close, successful bool is true. Close/shutdown repeated after completion succeeds without another close. An ordinary discarded result is **deliberate best effort**, not proof of delivery. There is no internal recursive error logger or hidden fallback destination.

## Default logger, ownership and reconfiguration

The module initializes a private default logger with memory-only default settings. Import resolution, source checking, LSP and cache population do not run it; execution performs ordinary Runtime-global initialization. Importing/logging aliases share that application Runtime's default. Independent applications/Runtimes have independent defaults. No file, directory, clock or console capability query is required merely to initialize it.

Instances own their file; they borrow standard streams. A name is just data: two independent loggers named `"game"` remain independent. Pass a mutable logger reference through fields/parameters for deliberate sharing. Calling state-changing logging/lifecycle operations through a deep readonly logger path is rejected by existing capability rules; a const alias does not freeze another mutable alias. Configuration copies do not copy file ownership. Two aliases of one logger share its busy/fault/closed state.

**Filesystem precondition:** one live writer owns each file object and managed namespace, including aliases. Two independent loggers/processes must not target the same file object through identical paths, hardlinks, case aliases or other path forms. No filename registry is introduced that would pretend textual path equality proves native identity. Distinct configurations must use distinct owned files. Console calls from independent loggers share the existing console serialization mechanism.

`configure` on an idle ready instance validates all settings first. If file policy/path is unchanged, metadata-only options can update without reopening; it cannot erase a prior file fault. Changing file policy closes/synchronizes the old owned file before acquisition of the replacement, avoiding two live aliases of the same output. Save both primary and cleanup errors. Opening or rotating can still leave filesystem changes; no configuration transaction or rollback is promised.

Same-namespace reconfiguration retains authority over previously owned exact members across close/reopen under the existing no-mutator/alias warranty; it does not require adopt merely to reopen its own file. Reject/adopt applies to newly introduced or unowned names. Revalidate retained files and reopen the active file for append, or create it exclusively if absent. This is retained application authority, not a new native identity check.

When reducing backup count on the same already-owned namespace, only previously owned exact slots above the new count may be removed, after preflight; never search for additional historical files. Increasing the set requires absent new slots or explicit adoption. Reject a reduced byte limit when existing files exceed it, before mutations. Path changes leave old completed logs in their old namespace. A newly opened smaller policy does not acquire or clean unknown higher-numbered historical names. The byte bound covers the **current managed file set**, not every file ever written under past configurations.

Successful replacement publishes settings only after acquisition. If close/mutation/open fails after external effects, leave the instance faulted, keep remaining ownership explicit and return failure; do not quietly continue under an old or half-new configuration. Ready/faulted instances can undergo explicit recovery configuration. Reusing a possibly torn active file requires explicit recovery retaining prior authority or explicit adoption, with valid LF termination; otherwise application repair/new destination is required. Never replay the whole record. An explicitly closed/deinitialized independent instance cannot reopen: acquire a new instance. Module `configure` can explicitly install a fresh default after shutdown, closing any previous one before detaching it. No implicit reopen on a later log call.

## Filtering, formatting and lazy costs

Threshold order is trace < debug < info < warn < error. No runtime log message parser or numeric coercion is added. All display/custom format hooks run through ordinary caller-side interpolation/string conversion.

```void
use log

func main():
    log.info("Game started")
    log.debug("Loading player")
    log.warn("Missing optional asset")
    log.error("Could not load configuration")
```

Those discarded results choose best effort. To require delivery:

```void
use log

func main() -> i32:
    if log.info("Game started") is Err(var problem):
        return 1
    return 0
```

`log.debug("Position: {position}")` evaluates the interpolation before entry even when filtered. Guard expensive work explicitly, or use the single lazy operation:

```void
use log

func main():
    var score = 42
    if log.enabled(log.level.debug):
        log.debug("Score: {score:000}")

    log.write_lazy(log.level.debug, func() -> string:
        return "Score: {score:000}"
    )
```

The lazy body runs zero times if filtered, exactly once if admitted and execution reaches it. Callback creation, capture cells, and any expression producing the callback still evaluate beforehand. A builder Runtime failure can prevent record emission; it does not become a fabricated logging Err. No allocation-free/zero-cost promise is made. The log-owned formatting path is shared by eager, lazy and async calls after a message exists.

Reserve the logger before invoking an admitted builder. Same-instance admitted recursive logging, close or configure returns busy; filtered valid writes may return false as above, and a different logger is usable. An ignored inner Err does not retroactively fail the outer call. Register the releasing guard before setting busy; release on normal return, propagation, Runtime unwind and cancellation. No user callback runs while a console/file native lock is held.

## Exact record and timestamp bytes

With timestamps, every successful record has this grammar:

```text
<UTC> <LEVEL> <quoted-name> <quoted-message><LF>
2026-10-28T09:10:11.123Z INFO "game" "Game started"\n
```

The displayed `\n` above denotes the single physical final byte `0A`. Actual bytes contain no trailing spaces or CR. Without timestamps, begin with LEVEL and one space. LEVEL is exactly TRACE, DEBUG, INFO, WARN or ERROR; no alignment padding. Always include the quoted name, even `""`. The same bytes go to console and file. No BOM, locale formatting, terminal colors or platform newline translation. Empty messages still produce a record.

Quote rules apply independently to name/message **after** ordinary formatting:

- Double quote → `\"`; backslash → `\\`; LF/CR/tab → `\n`, `\r`, `\t`.
- Other U+0000–001F, U+007F–009F, U+2028–2029, U+061C, U+200E–200F, U+202A–202E and U+2066–2069 → lowercase four-digit `\uXXXX` escapes (hex digits lowercase; the Xs here are notation).
- Other Unicode scalars retain their exact UTF-8 bytes, including emoji, combining marks and literal braces. Do not normalize text or interpret existing backslash sequences as escapes.

This prevents embedded record newlines and raw ESC/C0/C1 terminal commands and exposes specified bidi controls. It is not a general sanitizer for Unicode visual confusables, nor a JSON/object serialization API. A failed partial native transfer can leave a truncated escape or UTF-8 sequence; single-line/valid-record guarantees apply to completed records.

Sample time once per admitted prepared record, after the builder returns and before output. Reuse `time.now_utc` and `instant.format("yyyy-MM-dd'T'HH:mm:ss.fff'Z'")`. Fixed three-digit fractions truncate rather than round. UTC wall time can repeat/go backwards; do not use it as an ordering or unique-ID guarantee. Millisecond text does not claim millisecond clock resolution. Clock/format errors return clock failure before any sink output; there is no silent local-time or missing-timestamp fallback. With timestamps disabled, make no clock call.

Bound the final encoded byte array, including timestamp, quotes, escapes and LF. Check input-byte lower bounds before copying; check each append against remaining budget. Escape into one growable byte builder using existing UTF-8 validity, not repeated scalar rescans or quadratic string concatenation. Oversized records return `record_too_large` without truncation, rotation or sink writes. Caller interpolation/builder allocations are outside this bound; allocation failure keeps ordinary Runtime semantics.

## Console and file delivery

For an admitted, successfully framed record, attempt console first if configured, then the file independently even if console returns a recoverable error. File preparation/rotation failure does not cancel already completed console output. Preserve both causes and actual per-sink progress. Runtime failure/cancellation can interrupt this sequence; do not promise the second sink executes after an unhandled Runtime failure.

Use one `console.output.write_bytes(record)` call. That serializes a whole operation against other VOID console operations; it does not make stdout and stderr one external stream, sort by timestamp or prevent foreign processes/native writers from interleaving. Honor redirection as established by the console context. Never close the borrowed endpoint.

Hold a persistent `io.file`, use `write_bytes`/`write_bytes_async`, and retain active size from acquisition plus actual successful writes under the exclusive-writer precondition. Do not call path metadata on every record. Complete writes can still use multiple native chunks. No cross-process atomic append, exactly-once delivery across failures, or rollback of output to another sink is promised.

Any transport or rotation failure faults the logger after this attempt. Subsequent admitted writes return faulted without another transfer until explicit recovery; validation/clock/busy errors alone do not fault transport. This prevents silently appending after a torn record or replaying an entire partially written record. A reached guard conservatively faults after Runtime unwind/cancellation once transport or namespace mutation has started, even if the final transfer outcome is unknown. Never retry the entire record automatically.

## File paths, adoption and bounded rotation

Resolve a relative file path once through existing `path.absolute` at acquisition; store that absolute lexical path. Preserve Unicode/percent characters and native path errors. The parent must exist. No directory creation, ACL manipulation, symlink normalization or VFS remapping occurs inside log.

The managed set is the requested active path P plus exactly P`.1` through P`.N`. Backup 1 is newest. No directory enumeration, wildcard deletion or recursive cleanup. Files outside this explicit set are untouched. Known symlinks/reparse points, directories and special files are rejected by no-follow metadata preflight; archive operations never use `replace=true`.

**Reject mode:** preflight the entire set before mutation; any existing member is failure. Then `create_new` the active file. This avoids adopting unrelated existing files under the stated trusted-parent/no-mutator assumptions.

**Adopt mode:** the caller expressly authorizes managing those exact file objects. Existing members must be regular files, each at most rotation_bytes; empty is valid, otherwise require final byte LF using a bounded one-byte tail read. Refuse torn or oversized files; do not truncate, reinterpret, repair or delete them automatically. Missing active is created exclusively; existing active opens append. Gaps between archives are permitted. This validates layout, **not ownership**: the caller must guarantee trusted directory components, no hostile/concurrent name mutation, no other writers, distinct file objects and no unrelated hardlink/alias names. Current io cannot verify that warranty. `os.state_dir` is not a substitute for it.

To rotate before a record of R bytes, compare `R > rotation_bytes - active_length`, using checked bounds and no overflowing addition. R itself already fits the file bound.

1. Request file flush and close the active handle. Attempt close even after a recoverable flush error; stop before renames if either fails. Busy leaves the resource owned/retryable.
2. For N > 0, remove only the owned oldest P.N if present; move remaining existing slots descending with no replacement; move active P → P.1. For N = 0, remove only the owned closed active file.
3. Exclusively create a new active file, publish its owned handle, reset size to zero and write the already prepared record.

Stop on the first failed namespace operation, record that operation, retain the actual completed mutations and fault. Never copy/truncate as fallback, overwrite a collision or run compensating deletes. Renames preserve retained native file identities, but the sequence is not transactional. A crash can leave gaps/no active name; retention history may already have been removed before a later failure. No directory-sync/crash-durable rotation guarantee exists. Successfully managed current files stay within `(N+1)*rotation_bytes`; filesystem metadata, old namespaces, unadopted historical names and failed/torn records are not promised valid retained history.

## Flush, shutdown and failure details

`flush` calls the configured console's drain and owned file's existing flush, saving both recoverable errors. Console drain is not storage sync. File flush requests fsync/FlushFileBuffers; successful completion is an OS synchronization acknowledgement, subject to filesystem/device guarantees. No public directory-fsync facility makes rename history durable. There is no per-record sync or timer by default.

`close`/`shutdown` flush and then attempt file close even after recoverable flush Err. Close all actually owned resources; never return early just because console drain failed. Native close error still retires the underlying handle; busy does not. Finish closed after retirement, returning failures as appropriate. Do not erase resource ownership merely because a close was requested. A previously registered final-close fallback handles Runtime unwind using existing synchronous cleanup; cleanup cannot enqueue async work and may block.

`deinit` marks the source instance terminal before invoking checked synchronous close, and escalates Err through the existing checked Runtime-failure path, such as ordinary `convert.unwrap`. That diagnostic is generic; applications needing full native detail must inspect explicit close/shutdown results. Use a direct lifecycle method, not a Disposable interface or automatic destruction of nested fields. One-shot deinit is not an automatic retry system: cross-activation busy misuse may leave final release to Runtime provider draining and must report failure. Terminal state blocks emission/reconfiguration/reopen, but must not suppress a later explicit close retry on a still-owned busy resource; keep terminal admission separate from native-close completion.

`failure` retains inherited message/path/code plus:

- `kind: failure_kind`, `operation: string`;
- `console_cause: console.io_error?`, `file_cause: io.error?`, `clock_cause: time.error?`, `cleanup_cause: io.error?`;
- `console_attempted: bool`, `file_attempted: bool`, `console_complete: bool`, `file_complete: bool`, `console_bytes: i64`, `file_bytes: i64`.

Own fields should expose readonly diagnostic data. Disabled sinks have attempted/complete false; byte counts refer to this record, not previous output. Preparation/rotation can set file_attempted before any record bytes. Primary cause order follows operation order, with native OS code, partial bytes/UTF-16 units and cleanup information retained inside the original typed error. Log adds no separate message-content field; original transfer errors can contain partial record bytes, so failure objects are not a secret-redaction boundary. Allocation/Runtime failures may prevent publication of a typed failure after an external effect; the conservative fault guard and original Runtime diagnostic remain authoritative.

The GC does not run source destructors. Normal Runtime exit drains entered native work and closes remaining provider resources while roots live; it does not call logger shutdown or promise file synchronization. Explicit shutdown is required for a checked flush/close outcome. Abnormal process termination/power loss has no extra guarantee. No log-owned pending message queue can be lost at shutdown because none exists.

## Async, worker and reload behavior

Async variants await the existing `io` operations for acquisition, metadata/tail checks, rotation, writes, flush and close. They hold the same busy guard across suspension and retain prepared bytes/handles through ordinary traced frames. There is no additional queue: another admitted same-instance operation returns busy; it does not enqueue behind the first. Callers await sequentially or deliberately handle busy.

The console portion still uses synchronous console APIs and may block. These are async **file-operation** variants, not a promise of universally nonblocking output. Console-only logging needs no new async engine. Async calls evaluate arguments and enqueue before their bodies execute; threshold/configuration/default selection occurs when the body starts. Pending calls may observe intervening configuration. Always recheck filtering internally; an earlier enabled() call is not a reservation.

There is no async lazy-message argument or async function value. Use a normal caller-side guard/formatting step before `write_async`; the callee still filters. Workers cannot access log globals, owner logger graphs or console/files. Return safe message data to the owner through already approved mechanisms; no new channel/worker queue is implied.

Cancellation uses existing task semantics, not a new `Err(cancelled)`. Entered native jobs may already have effects; drain before final resource cleanup. Guard release must occur on cancellation even before any transport begins. `with logger` in an async activation drains its own child scope before synchronous deinit; aliases used by unrelated activations still need busy/ownership discipline. Explicit `close_async` avoids normal synchronous file flush/close in the owner continuation; its emergency cleanup remains synchronous.

Compatible reload preserves default and instance state/handles. Existing activations and closures retain leased code; changing config in source/global initializer, layouts or frozen initializer dependencies requires restart. Cached code contains no logger instance, file handle or timestamp; fresh application launches create fresh state. These are existing compiler/Runtime rules, not a logging cache or reload protocol.

## Portable location and cleanup examples

Directory creation remains visible application I/O. The example uses explicit adoption only where the application already owns the namespace and unrelated aliases are excluded:

```void
use log
use os
use path
use io
use convert

func main():
    var state = convert.unwrap(os.state_dir("com.example.game"))
    var directory = convert.unwrap(path.join([state, "logs"]))
    convert.unwrap(io.create_directories(directory))
    var destination = convert.unwrap(path.join([directory, "game.log"]))
    convert.unwrap(log.configure(log.config(
        name = "game", file_path = destination,
        existing = log.existing_files.adopt
    )))
    defer {
        convert.unwrap(log.shutdown())
    }
    log.info("Game started")
```

The deferred **block** matters: `defer convert.unwrap(log.shutdown())` would execute shutdown while evaluating unwrap's argument at registration. For recoverable cleanup, explicitly inspect shutdown's result instead of unwrap. os/path/io errors in setup keep their own types; log does not duplicate their APIs or silently guess another directory.

An independent file/console logger uses the same construction and cleanup:

```void
use log
use convert

func main():
    with convert.unwrap(log.open(log.config(name = "scene"))) as scene_log:
        scene_log.info("Scene loaded")
        var alias = scene_log
        alias.warn("Shared instance, same configuration")
```

An async usage example belongs in the existing explicit async-root host/runner, not an unsupported automatic async CLI-main promise:

```void
use log
use convert

async func entry(input: i32) -> i32:
    var scene_log = convert.unwrap(await log.open_async(log.config(name = "scene")))
    with scene_log as owned_log:
        var delivered = await owned_log.write_async(log.level.info, "Scene ready")
        var closed = await owned_log.close_async()
        if delivered is Err(var write_problem):
            return 1
        if closed is Err(var close_problem):
            return 2
    return input
```

## Approval boundary

Approve or revise the two owner choices in [RESEARCH](RESEARCH.md): explicit trusted filesystem ownership/adoption and checked failure/fault policy. The remaining semantics follow existing language/Runtime contracts or the concrete proposed module contract. This document does not authorize implementation, native capability expansion or hosted CI.
