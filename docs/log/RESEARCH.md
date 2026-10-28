# Logging: source audit and architecture research

28 October 2026. Baseline: **`f9ac49f1558f326129948bc06c829263448b7714`**.
Research only. [Proposed contract](DESIGN.md) · [Complete engineering plan](ROADMAP.md).
No logging implementation, compilation, execution, performance measurement or platform acceptance is claimed.

## Recommendation

Implement one ordinary source module, `stdlib/log.void`, over `console`, `io`, `time`, `path` and the established error/formatting machinery. Use one memory-only default logger per application Runtime and independent managed logger instances. Return checked results, use five severities, prepare one escaped UTF-8 record, and write it synchronously or through the existing async filesystem operations. No logging Runtime, native bridge, new scheduler, background writer, formatter or compiler intrinsic is needed for this proposed surface.

Use `log.failure` for errors: the requested `log.error(message)` function cannot coexist with a same-module `struct error`. Keep lazy callbacks synchronous and temporary, rather than fields in logger objects. This preserves the existing async boundary rules.

The substantive approval boundary is filesystem ownership. Current public `io` supports bounded rotation in an explicitly reserved, trusted, single-owner namespace. It cannot establish secure containment, detect hardlink aliases or condition deletion on a native file identity. Do not advertise source-only logging as providing those stronger properties. If Boss requires them, revise the existing `io` capability separately before claiming rotation complete under that threat model.

## Current contracts and historical status

Read the [Language Bible](../LANGUAGE_BIBLE.md), especially §§5, 8–10, 14, 17, 21, 29, 36–45 and 46; [Wrench workflow](../WRENCH_CLANKER.md); [delivery backlog](../VOID_STDLIB_BACKLOG_2026-10-27.md); [runtime globals](../RUNTIME_GLOBALS.md); [async](../ASYNC.md); [advanced closures/cleanup](../ADVANCED_LANGUAGE.md); [console contract](../stage10/CONSOLE_IO.md); [shared formatting](../stage10/DISPLAY_FORMATTING.md); [I/O contract](../io/CONTRACT.md) and [API](../io/API.md); [OS API](../os/API.md).

The backlog describes `os` and `collections` as future work at its recorded checkpoint. At this baseline their source and tests exist. The Bible's early paragraphs also defer facilities subsequently delivered, including full formatting and async I/O. Actual source and the later dedicated contracts determine availability. Historical LLVM/async incidents remain historical unresolved incidents; this module's future tests cannot retroactively prove them fixed.

## Verified source findings

References are baseline locations, not permanent line-number guarantees after engineering.

| Area | Current evidence | Consequence for `log` |
| --- | --- | --- |
| Standard endpoints | [console.void](../../stdlib/console.void), lines 5–20, 128–152: borrowed outputs, checked `write`, `write_bytes` and `flush` | Default to stderr; never close stdout/stderr or open a hidden terminal. |
| Console serialization | [console.cpp](../../src/runtime/console.cpp), lines 550–574 and 1022–1038: shared output gate and complete-transfer loop | Build the whole record first; make one console call. No lock around user message callbacks. |
| Console redirection | [CONSOLE_IO](../stage10/CONSOLE_IO.md), lines 364–373; console.cpp around 213 and 467 | Shell redirection works. Endpoint snapshots are established by the process context; arbitrary live host endpoint rebinding is not a logger feature. |
| Console flush | [console_abi.cpp](../../src/runtime/console_abi.cpp), lines 65–67 | Drain acknowledgement, not filesystem durability. No logger-owned stream buffer is needed. |
| File ownership | [io.void](../../stdlib/io.void), lines 356–454 | `io.file` is a managed identity with shared cursor/closed state and Runtime-owned native resource. It has a private constructor and explicit lifecycle. |
| File writes | io.void lines 557, 571, 1332; [filesystem.cpp](../../src/runtime/filesystem.cpp), lines 379–438 | Persistent `write_bytes` / `write_bytes_async`; short writes loop, failures retain actual progress. Native chunks are not an indivisible cross-process record. |
| File creation | io.void lines 125–146 and 420–448 | `create_new` acquires a fresh file without overwriting; `append` opens/creates at EOF. Avoid per-record `append_text`, which reopens/closes and does not explicitly flush. |
| File size and sync | io.void lines 640–713; filesystem.cpp lines 537–545 | `length` reads native size. `file.flush` requests `fsync` / `FlushFileBuffers`; it is stronger than console drain, but not unconditional power-loss durability. |
| Close errors | io.void lines 717–747; [filesystem_abi.cpp](../../src/runtime/filesystem_abi.cpp), lines 418–421, 456–473 | Native close failure retires the handle; busy preserves it. Never retry a retired close as if it still owned the same native descriptor. |
| Rotation operations | io.void lines 838–992; filesystem.cpp lines 939–995 | No-follow metadata, native no-replace rename, exact unlink and directory APIs already exist, including async forms. No copy/delete fallback is necessary. |
| Path/security boundary | filesystem.cpp lines 579–608 and 1207; [filesystem.hpp](../../include/void/filesystem.hpp), lines 37–44 | Public opens follow links/reparse points; metadata has no file ID/link count. Ordinary file permissions apply. Known-link rejection is a preflight check, not race-proof identity protection. |
| Portable locations | [os.void](../../stdlib/os.void), lines 215–227; [path.void](../../stdlib/path.void), line 7 | `os.state_dir` discovers a path; `path.join` composes it. Neither creates a private directory, verifies ownership or promises writability. |
| UTC and timing | [time.void](../../stdlib/time.void), lines 525–546 and 674–684; [temporal.cpp](../../src/temporal.cpp), lines 124–141 | Checked UTC sample and invariant temporal pattern formatting; monotonic time remains a measurement service, not the record timestamp. Actual errors are `time.error`, despite older string-error descriptions. |
| Shared display | [DISPLAY_FORMATTING](../stage10/DISPLAY_FORMATTING.md), default-display table; [compiler.cpp](../../src/frontend/compiler.cpp), interpolation lowering | Interpolation and ordinary display already support numeric/custom values. They evaluate before an ordinary logging call; log framing escapes the resulting string. |
| Lazy messages | compiler.cpp lines 815, 943–948, 2567 and 2637; [captures example](../../examples/advanced/captures.void), lines 27–29 | Exact synchronous `func() -> string` and captured block lambdas work. No variadic function/overload system is required. |
| Name collision | compiler.cpp lines 5286–5306 | Function/type names collide. Preserve `log.error`, name the result error `failure`. |
| Async boundary | compiler.cpp line 5853; [IR module.cpp](../../src/ir/module.cpp), lines 215–218 | Logger fields must not contain callables. An ordinary managed logger containing `io.file?`, strings/scalars/enums/errors is a viable async receiver. Lazy callback parameters cannot cross async task boundaries. |
| Cleanup | [ADVANCED_LANGUAGE](../ADVANCED_LANGUAGE.md), lines 99–128; compiler.cpp lines 2624–2634, 5650 and 5908–5912 | `deinit` is a managed, no-result, one-shot method. Deferred call arguments evaluate at registration; deferred blocks delay their bodies. Use reached cleanup before setting busy or acquiring ownership. |
| Error escalation | [convert.void](../../stdlib/convert.void), lines 51–55; [LLVM backend](../../src/backend/llvm/backend.cpp), lines 1323–1327 | `convert.unwrap(result<bool,failure>)` can surface cleanup failure, but its Runtime diagnostic is generic. Detailed native causes require inspecting the checked result. |
| Globals/reload | [RUNTIME_GLOBALS](../RUNTIME_GLOBALS.md), declarations, lifecycle and reload sections | Module aliases share one default cell; independent application Runtimes do not. Rebinding does not dispose the old logger. Compatible reload preserves cells/resources; incompatible layout/initializer changes require restart. |
| Owner and workers | IR module.cpp around line 1466; [ASYNC](../ASYNC.md) owner execution | Globals, console and files are owner-thread facilities. This module cannot introduce worker sharing or a worker-to-owner queue. |
| Shutdown | [coordination.cpp](../../src/runtime/coordination.cpp), around line 349; [abi.cpp](../../src/runtime/abi.cpp), around line 72 | Providers drain while roots remain live, including failed startup. This does not execute source logger `deinit` or guarantee a requested file sync. |

Do not depend on a nonexistent `Disposable` interface or implicit recursive field destruction. Give `logger` a direct lifecycle method. A static discrepancy between historical composition wording and the current promoted-method selector is unrelated to the proposed direct cleanup path; no composition change is required or authorized here.

## Complete capability ownership matrix

E = existing and reused; P = proposed `log` responsibility; B = owner policy boundary; O = outside this module.

| Capability | Owner | Status / exact boundary |
| --- | --- | --- |
| Five severity names and off threshold | log | P: trace/debug/info/warn/error; off filters everything but is not an emitted severity. |
| Default application facade | log + runtime globals | P/E: one private memory-only logger; no process singleton or registry. |
| Independent instances and labels | log + managed objects | P/E: aliases share one instance; equal labels do not share configuration. |
| Runtime filtering and enabled predicate | log | P: no formatting or clock calls inside filtered emission; ordinary caller argument evaluation still occurs. |
| Explicit lazy body | language callables + log | E/P: one `write_lazy(level,builder)`; zero/one body invocation, no saved callback. |
| Numeric/custom message formatting | language display/interpolation | E: no templates, format parser or compiler intrinsic in log. |
| Single-line quoting and record byte budget | log | P: framing policy after formatting, not another display engine or general serializer. |
| UTC sample/pattern; monotonic benchmark timing | time | E: distinguish nominal fractional precision from actual resolution and wall-clock order. |
| Standard stream transfer/serialization | console | E: stderr default, optional stdout/none, redirection preserved. |
| File handles, append, native transfers | io | E: persistent owned file, no log-native handle. |
| File sync and checked close | io + log lifecycle | E/P: aggregate errors, retain partial progress and close even after recoverable flush failure. |
| State/config/cache/home/temp locations | os | E: log does not duplicate directory conventions. |
| Path composition and absolute snapshot | path | E: no VFS reinterpretation or containment promise. |
| Directory creation and permissions | io / application | E/O: explicit application setup; no implicit private-directory claim. |
| Bounded size rotation and exact backup slots | log + io | P/E: pre-rotate, no-replace moves, explicit owned oldest deletion, no glob cleanup. |
| Existing-file adoption and namespace authority | application + log | B: reject by default; explicit adoption authorizes exact named objects under a trusted namespace contract. |
| Hostile path/identity protection | existing/future io | O/B: not provided by a logger path string or metadata preflight. |
| Async file operations | existing io/async + log | E/P: explicit awaitable emission/lifecycle, busy while pending; console portion remains synchronous. |
| Cancellation and native completion | Runtime | E: no new cancellation result or scheduler. Cleanup waits for already-entered jobs through existing machinery. |
| Reentrancy/configuration exclusion | log | P: per-instance source guard; another logger may be used. No lock while invoking user code. |
| Cross-thread / multi-process file writing | application/future sync | O: no new safe sharing; exclusive-writer precondition. |
| Async lazy callbacks and custom sink callbacks | language/future scope | O: not needed; current callable boundary prevents promising them. |
| Recovery diagnostics | log.failure + original modules | P/E: typed causes and sink progress; no recursive fallback logging. |
| GC roots, provider lifetime and code leases | existing Runtime/compiler | E: ordinary fields/frames/callback roots; no logging resource ledger. |
| Cache, compatible reload and fresh launches | existing compiler/Runtime | E: no serialized live file/callback/clock state. |
| Import docs, hover, signatures, navigation | compiler-owned voidls | E/P: ordinary documented source declarations; no editor-specific logging metadata. |
| Installation/relocation | CMake source installation | E: CMakeLists.txt line 205 installs all `*.void` sources. Add focused acceptance, not a new packager. |
| Background queues, automatic periodic flush, network sinks | future/application | O: no unbounded queue, scheduler or provider plugin system. |
| JSON logging, spans, hierarchical categories, automatic source locations | optional future | O: not necessary to fulfill the proposed text/file module. No reflection, stack capture or logging syntax added. |
| Compiler diagnostics/build timings | compiler/tooling | O: explicitly outside application `log`. |

## Alternatives and source-first choices

### Formatting and lazy evaluation

The simplest useful API accepts an already rendered string. Preserve caller-owned visibility of custom display hooks: formatting in the caller avoids generic-library hook-discovery surprises. Add `enabled(level)` for explicit guards and one `write_lazy(level, func() -> string)` for convenient deferred construction. A factory producing that callback and capture/environment construction still execute before the call. Allocation/resource failures in interpolation or a builder retain the language's Runtime-failure behavior; do not convert every failure into a fictitious recoverable logging error.

Go documents eager argument evaluation and provides deferred values; Rust has explicit admission checks; Microsoft likewise recommends guarding expensive arguments. These support explicit deferral, not a promise that an ordinary function can undo argument evaluation. [Go performance guidance](https://go.dev/pkg/log/slog/?m=old), [Rust log](https://docs.rs/log/latest/log/), [.NET library guidance](https://learn.microsoft.com/en-us/dotnet/core/extensions/logging/library-guidance).

Do not store formatter/sink callbacks merely for flexibility: they would make the logger graph fail current async boundary checks. A fixed framer plus two existing sink kinds meets the mission with less machinery. A small log-specific escaping helper is justified: ordinary display preserves raw controls, while JSON's general serialization API is a different contract and dependency. Use existing UTF-8 guarantees and byte arrays; avoid scalar indexing in a loop that repeatedly rescans from the beginning, or repeated whole-record string concatenation.

### Errors and lifecycle

Recommend checked results for all convenience methods. Discarding one deliberately selects best effort; it does not mean delivery succeeded. Go discards handler errors, while Python/spdlog have their own diagnostic policies. VOID should retain its normal recoverable errors and avoid recursively reporting a failed log through itself. [Go handler contract](https://go.dev/pkg/log/slog/?m=old), [Python handlers](https://docs.python.org/3/library/logging.html#handler-objects), [spdlog errors](https://github.com/gabime/spdlog/wiki/Error-handling).

No source-owned output queue/buffer is necessary: transfers already write directly. `flush` therefore delegates console drain and file synchronization. A logger close can save a recoverable flush error, still attempt native close, and return both causes. A Runtime failure needs an already registered cleanup fallback; cancellation cleanup cannot start async work. Explicit checked close is the path for detailed error handling, with a synchronous destructor fallback for `with`.

### Rotation and genuine security limits

Unlike common rotation thresholds that allow a record to overshoot, the proposal rejects an encoded record over budget before writes and counts every UTF-8 byte, quotes, timestamp and LF. Under exclusive ownership, active plus N backups remain bounded. Python disables rollover for particular zero settings; spdlog permits oversized writes after rotation. Those policies are precedents, not the proposed VOID guarantee. [Python rotation](https://docs.python.org/3/library/logging.handlers.html#rotatingfilehandler), [spdlog implementation](https://raw.githubusercontent.com/gabime/spdlog/v1.x/include/spdlog/sinks/rotating_file_sink-inl.h).

There is no atomic multi-rename transaction. Close before moving for Windows compatibility; use same-directory no-replace rename, never cross-volume copy/truncate. A failed move can leave gaps and an absent active name. Report the actual step and stop; do not roll back with speculative overwrites/deletions. The namespace and native file sizes remain authoritative when a human/app explicitly chooses recovery.

Metadata regular-file checks cannot prove ownership. Even without a racing attacker, a hardlink can alias an unrelated file. Default rejection plus exclusive creation avoids adopting pre-existing final objects under trusted-parent/no-mutator assumptions. Explicit adoption requires application authority over the objects and a warranty of distinct, unrelated-alias-free file identities. If that precondition is unsuitable, the current public APIs are insufficient for the stronger guarantee. Do not silently add a manifest registry, secure-temp subsystem or log-native resource layer to conceal the gap.

## Reusable verification and performance evidence

Existing infrastructure already provides:

- [filesystem hooks](../../include/void/detail/filesystem_hooks.hpp): short chunks, zero writes, partial failure after N bytes, close errors and operation barriers, retained safely by native requests. [Kernel tests](../../tests/io/kernel.cpp), around 182–223, exercise those paths.
- [console source tests](../../tests/stage10/console.cpp) and [Runtime console tests](../../tests/runtime/console.cpp): injectable endpoints, record/progress/failure behavior.
- [managed probe](../../tests/managed/probe.hpp), [I/O ownership](../../tests/io/ownership.cpp), [native-job lifecycle](../../tests/io/native.cpp): forced GC, allocation budgets, barriers and cleanup/shutdown races.
- [I/O runner](../../tests/io/runner.cpp): explicit async root compilation and emission; ordinary CLI main is not automatically an async root adapter.
- [collections execution/support](../../tests/collections_stdlib/execution.py), [cache](../../tests/collections_stdlib/cache.py), [host](../../tests/collections_stdlib/host.cpp), [LSP](../../tests/collections_stdlib/lsp.py), [installation](../../tests/collections_stdlib/install.py): existing patterns to reuse, with logger-specific assertions.
- [crash recorder](../../tests/text_encoding/crash.py): launch-time exact source snapshots, `_crash.void`, commands, native statuses, sanitizer findings, stderr/stdout and hashes. Do not create another incident recorder.

No log implementation exists to benchmark during this mission. Performance risks to measure are eager formatting despite filtering, capture allocation, escaping growth, UTF-8 copies, per-record task/native-job allocation, file synchronization cost and O(N) rotation operations. Maintain a byte count under exclusive ownership rather than calling native metadata/length for every record. No hidden formatting, clock sampling, transfer or task creation is needed inside the filtered synchronous path. This is a design property to verify, not a zero-cost claim.

Bound only module-owned record buffers and current retention slots. Input strings/builders can allocate before admission; existing task/result record retention within a long-lived async root can grow with total calls ([ASYNC](../ASYNC.md)). No queue in `log` is not proof of constant process RSS. Native filesystem latency and disk-full behavior require actual platform results.

## Genuine owner decisions

1. **Filesystem ownership:** approve trusted, application-reserved, exclusive-writer namespaces with default reject/explicit adopt and documented alias limits; otherwise specify the stronger existing-`io` enhancement before promising rotation under hostile namespaces. Recommendation: source-only contract for ordinary application logs; no secure-directory claim.
2. **Failure contract:** approve checked convenience methods and conservative faulting after transport/rotation failure, with explicit recovery and no hidden stderr fallback. Recommendation: checked results; callers may deliberately ignore them for best effort.

Defaults and the exact API are proposed together in DESIGN, not a list of new language decisions. Everything remains subject to Golden Wrench review and Boss approval. No production, Git, hosted CI or protected root README changes occurred in this research.
