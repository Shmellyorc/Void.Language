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

`use text` provides scalar-indexed literal search and extraction, Unicode whitespace
trimming, splitting/line splitting/joining, replacement, repetition and scalar-width
padding. `to_lower`/`to_upper` apply pinned Unicode 16.0 full default casing, including
expansions and final sigma, without locale tailoring or normalization. Fallible
construction APIs return managed errors and bound output to 64 MiB.

`use encoding/hex` and `use encoding/base64` provide plain-text `encode`, recoverable
`try_encode` for output limits, and strict result-returning `decode` into independent
owned bytes. Hex is strict ASCII pairs, with explicit uppercase output. Base64
selects standard/URL-safe alphabet and required/omitted padding explicitly; decoding
rejects whitespace, mixed alphabets, malformed padding and nonzero unused bits.
`use encoding/ascii` provides strict lossless 7-bit conversion, byte predicates and
ASCII byte casing. NUL is data in every module. Allocation exhaustion follows the
existing checked Runtime failure channel. See the [text/encoding contract](../docs/text-encoding/CONTRACT.md),
[API inventory](../docs/text-encoding/API.md) and [engineering record](../docs/text-encoding/IMPLEMENTATION.md).


## Gameplay randomness

`use random` provides version-1 xoshiro256** streams expanded from exact u64
seeds with SplitMix64. `var world = random.generator(seed = 42)` creates a
mutable value stream; assigning it copies its current position independently.
`world.seed(42)` resets it. Module `random.seed(42)` sets a shared per-Runtime
default; otherwise its first needed draw lazily seeds from eight OS bytes.
Explicit generators never request entropy. This module is for gameplay,
never cryptography.

For every integer type and f32/f64, `rand_T()` / `rand_T(maximum)` /
`rand_T(lower, upper)` draw full-domain integers or unit floats, [0,maximum),
and [lower,upper), respectively. Exact typed operands are required; literals
adapt to the chosen type. `rand_range(lower,upper)` is integer-only and
inclusive, including full i64/u64 extrema. `boolean()` uses one high bit.
`try_rand_T` handles argument/entropy failures recoverably; explicit generator
checked methods take one or two bounds. Default checked zero-argument draws,
`try_boolean`, `from_entropy` and `try_initialize` expose seed failures.

`choose` / `try_choose`, in-place `shuffle`, and copy-returning `shuffled`
operate on growable lists; defaults also offer `try_shuffle` / `try_shuffled`.
Fixed arrays require explicit slicing. Const-input choices and shuffled copies
preserve readonly referenced children and their identity; value elements copy.
Invalid calls and integer singleton intervals do not consume a word, and default
singletons skip entropy. Finite float bounds must form a nonempty interval.
Module functions as values keep their declared zero-argument signature;
bounded forms are direct-call syntax, rather than general function overloads.

[Complete contract](../docs/random/DESIGN.md),
[API inventory](../docs/random/API.md),
[verification](../docs/random/IMPLEMENTATION.md),
[replay example](../examples/random/gameplay.void).
