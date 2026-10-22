# Standard library

`use time` provides validated Gregorian dates, times, local/offset/zoned values,
full-range seconds-based instants and durations, checked arithmetic, bundled IANA
rules and UTC/monotonic clocks. Components are private const fields with public
read-only properties and recoverable factories. Formatting uses the shared Runtime
kernel through ordinary source display/format hooks. Clock precision queries report
native resolution in nanoseconds, not UTC accuracy. Monotonic samples belong to
one Runtime context; compare/elapsed reject mismatched contexts.

The compiler owns only narrow checked native operations; library policies and
public values use ordinary VOID source. Installed CLI/LSP tools discover matching
sources under `share/void/stdlib`, with `VOID_STDLIB_ROOT` as an explicit override.
Time-zone rules are embedded in linked Runtime bytes. Stage 10 remains incomplete;
see `docs/stage10/IMPLEMENTATION.md` for exact acceptance and remaining systems.
