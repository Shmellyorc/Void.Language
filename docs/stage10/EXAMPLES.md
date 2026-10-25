# Stage 10 runnable examples

[Console/time example](../../examples/stage10_console_time/main.void) combines
private const storage, direct owner initialization, an exact owner operator,
captured-place `+=`, a custom display hook, numeric/temporal interpolation,
recoverable date construction, heterogeneous print and explicit UTF-8 output.
It needs no interactive terminal, ambient time zone or current clock value.

From the repository root, substitute the actual built compiler for `voidc`:

```sh
voidc check examples/stage10_console_time/main.void
voidc run examples/stage10_console_time/main.void --no-opt
voidc run examples/stage10_console_time/main.void
voidc build examples/stage10_console_time/main.void -o /tmp/void-stage10-example
/tmp/void-stage10-example
```

All execution forms return zero, with empty stderr and these exact UTF-8 bytes
(LF after each line):

```text
VOID | score:012 | 2024-02-29
UTF-8: é🎮
```

Explicit `flush = true` belongs to print. Low-level `write` returns a recoverable
result; print itself uses the established checked failure/cleanup path. Borrowed
stdout/stderr are never closed by application source. A custom equality operator
would exclude this owner recursively from dictionary keys; this example declares
only arithmetic and does not imply custom-key hashing support.

The earlier [fields/display example](../../examples/stage10_fields_display/main.void)
retains the legacy `entry(input: i32) -> i32` contract. With input 7 its return
value is 23. Legacy CLI entry-result text is separate from application stdout.
For more exhaustive scenarios use existing fixtures and their asserted runners:

| Area | Executable regression fixture / driver |
|---|---|
| All four systems, GC and failures | `tests/stage10/console_operator_systems.void`, `operator_console_model.void`, `operator_integration.cpp` |
| Captured compounds and async | `operator_systems.void`, `operator_systems_async.void`, `operator_compounds.cpp` |
| Redirected prompts, partial transfers and native modes | `console.py`, `console_terminal.void`, `console_keys.void`, `tests/runtime/console_*.py` |
| Formatting, composite values and temporal zones | `conversion.cpp`, `interpolation.cpp`, `temporal.py`, `operator_keys.void` |
| Compiler-owned tooling | `lsp_intelligence.py`, `lsp_source_formatting.py`, `tests/editors/run.py` |

Paths in the table are relative to `tests/stage10/` unless explicitly prefixed.
Fixtures are test inputs; some intentionally return nonzero values or exercise
failure paths and should be run through their existing assertions.

Local closeout records contain exact commands, output hex and compiler hashes:
[check/JIT](engineering/closeout/example-attempts.json),
[native builds and design probes](engineering/closeout/example-native-and-design-probes.json),
[build provenance](engineering/closeout/example-identities.json). Existing local
Debug CI96 and Release Step41 compilers were reused; no fresh compiler build or
additional full-suite pass is claimed. The example was added after experiment #4.
Strict ASan/UBSan/leak checking and both JIT modes also pass with the existing
CI96 sanitizer compiler; [exact environment/output](engineering/closeout/example-strict.json).
Generated application instructions are not equivalently sanitizer-instrumented.
The [existing example rerun](engineering/closeout/existing-example.json) retains
its exact `23` legacy result in both compiler configurations and JIT modes.

Current application selection accepts zero-argument synchronous `main()` with
either no return value or `i32`. A `[string]` parameter declaration checks as an
ordinary function but is not selected as an application entry today. A focused
run returned `VOID4001: No entry function found`. Nullable `is Ok(var value)`
**is supported** and passed a focused execution probe; it is not restricted to
`result` values. These observed facts correct assumptions in the future-design
review without authorizing new entry/global semantics.
