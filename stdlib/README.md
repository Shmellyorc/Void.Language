# Standard library

`use io` provides owned file handles, bounded whole-file bytes/UTF-8 reads,
write/append, metadata, file and directory copy/move/removal, symbolic links and
filtered recursive directory lookup. `get_files(directory, "*.bin", true)` and
`get_directories(directory, "save*", true)` return ordinary owned arrays.
`entries` returns type, name, size and modification metadata, including links.
Every blocking operation has a paired `_async` API backed by the existing bounded
native worker queue. Await entered work before closing; conflicting aliases
receive `busy`. Cancellation drains an entered syscall before cleanup.

`use path` supplies host-aware joining, lexical normalization, filename/stem/
extension/parent/root extraction, extension replacement and absolute/relative
conversion. These operations do not resolve symlinks or establish existence.
Both modules use recoverable `io.error`, derived from the common error base.
Errors preserve native codes, actual transfer progress, owned partial bytes and
cleanup failures. Writes and recursive mutations can leave partial changes.

```void
use io
use path
use encoding/binary

func save(score: i32) -> result<i64, io.error>:
    var name = path.join(["saves", "player.bin"])?
    var made = io.create_directories("saves")?

    with io.open(name, io.open_mode.create, binary.byte_order.big)? as handle:
        var written = handle.write(score)?
        return handle.write_pascal_string("雪")
```

File aliases share one logical cursor and closed state. File construction uses
`open`/`open_async`; close is idempotent. Scope disposal is synchronous: explicitly
await `close_async()` before leaving an async scope when disposal must stay off
the owner. GC does not invoke source destructors; forgotten handles remain owned
by their Runtime until shutdown. Append writes always select EOF, even after a
seek. File seeks may pass EOF; memory seeks stay within existing data.

File numeric/raw/Pascal methods match memory wire representations and reuse
`encoding/binary`. Pascal prefixes count UTF-8 bytes as a u32 in the selected byte
order. Raw strings have no implicit framing or newline. Text decoding is strict.
Whole-file and line reads default to a configurable 64 MiB budget. Enumeration
defaults to 100,000 inspected entries and never intentionally traverses links.
See the [full contract](../docs/io/CONTRACT.md), [API audit](../docs/io/API.md) and
[executable configuration/save/asset example](../examples/io/filesystem.void).

`use error as core` exposes the three-field managed `core.error` base. Every
installed module uses a derived public recoverable error; result error types
remain exact. `use convert` provides checked numeric/Boolean/payload-free-enum
parsing, ordinary `to_string`, checked `unwrap` and eager `unwrap_or`.
`use console` adds ordinary `readline(prompt = "")` and `read(max_scalars = 1)`
convenience over the checked console queue. See the
[current combined contract](../docs/combined/CONTRACT.md) for grammars, errors,
ownership, console coverage and performance guarantees.

`use io/memory` provides an owned `memory.stream(bytes = [], order = ...)` with a
shared cursor, independent `to_bytes()` snapshots, bounded seeking, up-to/exact
byte reads, numeric `read<t>()`/`write(value)` through the binary codec, strict raw
UTF-8 and u32-length-prefixed Pascal strings. Failed operations preserve the old
state. Writes reuse geometrically grown private backing; see [the contract](../docs/memory/CONTRACT.md) and
[local implementation evidence](../docs/memory/IMPLEMENTATION.md) for exact costs,
tests and review limits.

`use encoding/binary` provides `encode(value, order)` and `decode<t>(bytes, order)`
for all ten fixed-width numeric primitives. Little endian is the default; explicit
big endian is supported. Encoding returns fresh owned bytes, decoding requires an
exact width and returns a recoverable managed error with
`kind == error_kind.wrong_width`, and IEEE bits are preserved.
See [the binary contract](../docs/binary/CONTRACT.md) and
[implementation evidence](../docs/binary/IMPLEMENTATION.md).

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
Time-zone rules are embedded in linked Runtime bytes. Stage 10 and Stage 11 locks
remain separate from this standard-library sprint.
