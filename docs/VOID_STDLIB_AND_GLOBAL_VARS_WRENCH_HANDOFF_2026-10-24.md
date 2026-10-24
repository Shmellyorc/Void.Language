# 🔧 VOID 2.0: Standard Library and File-Level `var` Design Handoff

**To:** Golden Sacred Wrench / Wrench Clanker  
**From:** Boss and the standard-library design discussion  
**Date:** 24 October 2026  
**Purpose:** Independent architecture and ergonomics review. **Discussion/design direction only. No implementation, CI, Git action, or final API approval is authorized by this handoff.**

## 0. What Boss is asking you to review

We brainstormed a broad, practical VOID 2.0 standard library and one language-level addition: **file-level mutable runtime `var` declarations in any `.void` file**. Please assess them against the *current* Language Bible, existing compiler behavior, type system, VFS/import model, GC, native JIT/AOT, hot reload, and Stage 10 progress. Point out conflicts, bloat and semantic gaps before recommending an implementation contract. Preserve the distinctions between **owner-favored direction**, **illustrative spelling**, and **already implemented** behavior.

The governing product rule is: **“Make VOID easy to use; let the compiler/runtime/stdlib handle complexity, not the application developer.”** Equally important: convenience must not remove lower-level control, add ambiguous implicit conversions, or recreate C# wholesale. KISS, DRY, one semantic model, and the existing VOID compiler/Runtime apply. Keep each module focused. Examples below are **VOID-flavored pseudocode**, not claims that these calls currently compile.

Our wider Stage 10 language-completeness mission precedes substantial standard-library implementation. Boss retains all architectural sign-off, Git and hosted CI control. Wrench reviews; Keyboard implements only when separately authorized. Do not modify the root README or treat this report as a sprint order.

## 1. Module organization and design principles

We can use the VFS `use` system and the existing same-named folder entry convention, rather than inventing new import grammar:

```text
stdlib/
  sync/sync.void       # use sync
  sync/guard.void      # use sync/guard
  sync/mutex.void      # use sync/mutex
  net/http.void        # use net/http
  net/tcp.void         # use net/tcp
  net/udp.void         # use net/udp
  encoding/binary.void # use encoding/binary
  encoding/base64.void # use encoding/base64
```

These are candidate public layouts, not mandatory physical files for every helper. Modules can share internal kernels. Prefer one obvious home for an operation, and provide fluent/type-level conveniences only where they earn their existence. Reuse `result<t,e>`, `Ok`, `Err`, `?`, `is`, `match`, `with`, generics, closures and type inference. **No `out` parameters.** Existing built-in language features should not be duplicated in the library just to look familiar.

## 2. `console`: straightforward interactive I/O

The motivating example needed `c.stdin.read_line()`, a nested `text_read` pattern and manual unwrapping just to ask for text. The low-level API can remain for advanced stream handling, but ordinary input should be direct:

```void
use console

var name = console.input("Name: ")
print("Hello, {name}!")
```

Also consider a recoverable form such as `console.try_input(prompt) -> result<string, ...>`. Do not commit both names until EOF, I/O failure and noninteractive input behavior are defined. `console.print` is possible, but **VOID already has `print()`**; avoid a duplicate unless a clear benefit exists. Let the stdlib handle prompting and flushing sensibly.

## 3. `convert`: textual parsing and ordinary value conversion

Explore module-level and fluent calls backed by **one parser/conversion implementation**:

```void
use convert

var level = convert.parse<i32>("42")?
var amount = "42".parse<i32>()?
var label = amount.to_string()
# Possible type-level convenience: i32.to_string(amount)
```

Cover `i32`, `i64`, unsigned integers, `f32`, `f64`, `bool`, and supported enums. Keep parsing a textual representation distinct from decoding fixed-layout binary bytes. Preserve `result`/`?`; do not silently unwrap errors or turn arbitrary string data into unchecked numeric `as` conversions. Type-level and instance call styles are illustrative, not approval for universal method-call syntax or three parallel parsers.

## 4. `encoding`: byte representation, not value parsing

Boss specifically wants converting byte arrays to/from strings **and numeric primitive representations**. Proposed families include UTF-8, ASCII, Base64, hex and binary numeric encoding:

```void
use encoding

var bytes = encoding.utf8.encode("Hello")
var text = encoding.utf8.decode(bytes)?
var ascii = encoding.ascii.encode("Hello")?
var b64 = encoding.base64.encode(bytes)
var decoded = encoding.base64.decode(b64)?
var hex_text = encoding.hex.encode(bytes)

var number = encoding.binary.decode<i32>(raw, byte_order = encoding.byte_order.little)?
var raw = encoding.binary.encode(number, byte_order = encoding.byte_order.little)
```

`encoding/binary` can be imported explicitly. Require a documented byte order, exact byte widths, strict UTF-8 decoding and a defined ASCII failure policy. Distinguish ASCII text bytes `[49,50,51]` (decode text, then parse `123`) from the four-byte little-endian integer `[123,0,0,0]` (binary decode). Share the same primitive codec with `io` binary readers/writers.

## 5. `io`: text, bytes, files, binary streams and memory streams

Support whole-file convenience, incremental I/O, typed binary readers/writers and in-memory packet construction. **The same reading/writing semantics apply whether backed by a file or a memory buffer.**

```void
use io

var text = io.read_text("config.txt")?
var bytes = io.read_bytes("image.png")?
io.write_text("note.txt", text)?
io.write_bytes("backup.bin", bytes)?

with io.open_read("data.txt")? as file:
    for line in file.lines():
        print(line)

var packet = io.memory_stream()
packet.write_u8(1)?
packet.write_i32(500)?
packet.write_f32(12.5)?
var encoded_packet = packet.to_bytes()

var reader = io.memory_reader(encoded_packet)
var kind = reader.read_u8()?
var count = reader.read_i32()?
```

Support `u8`, other integer widths, `f32/f64`, exact reading, endianness and predictable truncation/EOF failures; make byte order configurable. Files/memory streams must not need separate codecs. Streaming should not load large files unnecessarily. Resource lifetimes should use the existing `with`/`deinit` semantics.

**Boss specifically favors two different string encodings in binary writers**, inspired by Godot:

```void
stream.write_string("Hello")?         # Just 5 UTF-8 bytes. NO length/terminator/newline
stream.write_pascal_string("Hello")?  # Byte-length prefix followed by UTF-8 bytes

var first = reader.read_string(5)?
var second = reader.read_pascal_string()?
```

Raw `write_string` must **not** silently use Pascal framing. A candidate Pascal representation is `u32` byte count with defined little-endian prefix; exact width, byte order and maximum size are **still open**. The prefix counts UTF-8 *bytes*, not Unicode scalars. Validate truncation, length overflow, malicious lengths and UTF-8. Developers creating custom packet formats must remain free to write their own `u8/u16/u32` length followed by raw string bytes.

For applicable filesystem operations, Boss expressly wants paired blocking and asynchronous names, e.g. `io.read_bytes()` / `io.read_bytes_async()`, `io.write_text()` / `io.write_text_async()`, `io.open_read()` / an appropriate async counterpart. `_async` communicates possible suspension instead of surprising callers. Preserve correct `await` and `result` semantics, e.g. `(await io.read_bytes_async(path))?` if the async result carries a result value.

## 6. `random`: deliberately distinct exclusive and inclusive APIs

**Preserve both forms on purpose.** Boss does not want them collapsed as redundant:

```void
use random

random.boolean()
random.rand_i32()         # Full/default typed range, exact contract pending
random.rand_i32(10)       # 0..9: EXCLUSIVE upper bound
random.rand_i32(5, 10)    # 5..9: EXCLUSIVE upper bound
random.rand_i64(items.count)  # index-friendly: count is i64

random.rand_range(1, 6)   # 1..6: INCLUSIVE endpoints, dice/game values
random.rand_f32()
random.rand_f32(10.0)
random.rand_f32(5.0, 10.0)
```

Explore matching integer widths, unsigned and floating types. `rand_range(min,max)` should infer/accept numeric types and be inclusive for integers; **do not implement floating-point inclusivity by `max + 1`**. Floating endpoint and distribution policies need exact review. Also consider `random.choose(items)`, `shuffle` and a seeded generator for repeatable games. Handle zero/empty bounds and integer overflow correctly. Gameplay `random` must remain separate from security randomness (`crypto`).

## 7. `math`: one numeric module

Do not copy C#'s separate `Math` and `MathF` families. Rely on the existing VOID numeric types and consistent typing:

```void
use math

health = math.clamp(health, 0, 100)
var distance = math.abs(-20)
var lo = math.min(a, b)
var hi = math.max(a, b)
var point = math.lerp(start, end, 0.5)
```

Consider rounding, trigonometry and vector `.length()` where a concrete type/interface warrants it, but don't make arbitrary structs magically support length. Preserve overflow, float and conversion semantics; avoid lossy implicit mixes. Common fluent methods are optional, not a license for a second API implementation.

## 8. `text`: useful operations with correct Unicode behavior

```void
use text

var name = "  Hero  ".trim()
var parts = "red,green,blue".split(",")
var combined = text.join(parts, " | ")
var short = "Hello World".substring(0, 5)
var yes = name.contains("Hero")
var new_text = name.replace("Hero", "Mage")
```

Also consider `starts_with`, `ends_with`, case conversion and emptiness. Reconcile `substring` with the Bible's Unicode-scalar indexing and `byte_length` rules; never silently mix scalar positions with byte offsets. Keep byte-oriented operations under encoding/buffer APIs.

## 9. `enum` and `flags enum`: typed convenience without forbidding bitwise operations

Explore enum text/name/numeric conversions and metadata, while preserving distinct enum types:

```void
enum action:
    attack = 1
    heal = 2
    run = 3

var options = enum.values<action>()
var chosen = enum.parse<action>("attack")?
var valid = enum.is_defined<action>(2)
var raw = action.attack.value
# Candidate checked numeric -> enum: 2 as? action
```

`enum.parse<t>` could accept names, numeric spellings or both; this must be specified. Data-carrying enums must not silently be treated as plain integer enums. `action.attack` must not implicitly equal an unrelated enum or the integer `1` solely because they share a representation.

**Boss likes the proposed `flags enum` declaration** and methods that do the ordinary bitwise work, while retaining manual `&`, `|`, `^`, `~` and assignment operators for advanced callers:

```void
flags enum ability:
    none = 0
    jump = 1
    run = 2
    swim = 4

var abilities = ability.jump | ability.run
var can_jump = abilities.has(ability.jump)
var some = abilities.has_any(ability.swim | ability.run)
abilities.add(ability.swim)
abilities.remove(ability.run)
abilities.toggle(ability.jump)
# Alternative immutable value-returning forms: with(...) / without(...)
```

`has` for flags (all requested bits) must be distinguished from compile-time `Type has Interface/Component` and `enum.is_defined` for declared values. Decide zero-mask behavior, composite flags, whether methods mutate or return new values, and typed vs backing-integer operations. Keep a shared bitwise implementation.

## 10. `json`, CSV, TOML, XML and dynamic lookup

Boss wants both typed JSON conversion and easy dynamic lookup **without C# `out` parameters**:

```void
use json

var player = json.parse<player_data>(text)?
var saved = json.stringify(player)?

var root = json.parse(text)?
if root.get("player") is Ok(var node):
    if node.get("name") is Ok(var name_node):
        print(name_node.as_string()?)
```

A `get` yielding `result` may already supply the useful “exists and capture” behavior; don't automatically add overlapping `try_get`. Differentiate absent field, explicit JSON `null`, wrong type and malformed document. Also explore `data/json`, `data/csv`, `data/toml`, `data/xml` folder families **without building two JSON libraries** or a giant mandatory serialization framework. Include sensible bounds for untrusted input.

## 11. Built-in `[]` / `{}` and optional collection algorithms

Preserve VOID's Lua/Godot-style growable/fixed arrays and dictionaries instead of inventing `List<T>`, `Stack<T>`, `Queue<T>` and LINQ's full query infrastructure. Current `push`, `remove_at` and `.count` should remain the basic operations. The Bible has historically deferred `.pop()` pending an exact `result` signature; verify current status before claiming otherwise.

Boss favors:

```void
items.sort()
items.sort(reverse = true)
enemies.sort(func(a, b): a.health < b.health)
# Optional later: enemies.sort_by(func(e): e.health)
```

The comparator uses `bool`, with `true` meaning a precedes b. Infer `a` and `b` types from the list's generic element type where feasible; no comparer class or redundant `<t>` annotation for ordinary lambdas. Require coherent ordering, preserve readonly/fixed-value-array semantics, and decide stable sorting/allocations explicitly. Other potential conveniences include `contains`, `find`, `filter`, `map`; no eager commitment to a full LINQ clone or unclear lazy allocation model.

## 12. Networking: **separate protocols**, organized under `net/`

Boss corrected the layout to exploit VOID's VFS imports:

```void
use net/http
use net/tcp
use net/udp
```

HTTP handles web requests/responses and headers; TCP handles reliable byte streams; UDP handles datagrams. Do **not** put them behind one protocol-ambiguous public router. Share safe low-level transport infrastructure internally where justified. Typical use cases:

```void
var response = http.get(url)?
var response = (await http.get_async(url))?

with tcp.connect(host, port)? as connection:
    connection.write_bytes(packet)?

with udp.bind(port)? as socket:
    socket.send_to(packet, destination)?
```

A packet produced by `io.memory_stream()` should be usable by TCP/UDP. No mandatory sockets/TLS knowledge for the easy HTTP path. Provide explicit `_async` variants where meaningful; don't misrepresent TCP byte streams as UDP message boundaries.

## 13. `path` and `os`: paths versus machine/application environment

```void
use path
use os

var full = path.join("assets", "player.png")
var name = path.filename(full)
var ext = path.extension(full)

var system = os.name
var newline = os.newline       # Windows \r\n; modern Linux/macOS \n
var data = os.dirs.app_data("my_game")?
var cache = os.dirs.cache("my_game")?
var config = os.dirs.config("my_game")?

if os.app.is_bundle():
    var resources = os.app.resources_dir()?
```

Include OS-aware app data/config/cache locations (Windows Local vs Roaming where appropriate, Linux XDG, macOS Application Support) and **macOS `.app` bundle detection**. Running on macOS does not imply running in a bundle. Do not confuse source `use` VFS paths with host filesystem paths. Platform newline is opt-in; portable text need not silently rewrite `\n`.

## 14. `compression`: both memory and files/streams

```void
use compression

var packed = compression.gzip.compress(bytes)?
var unpacked = compression.gzip.decompress(packed)?

with io.open_read("save.dat")? as source:
    with io.open_write("save.dat.gz")? as destination:
        compression.gzip.compress_stream(source, destination)?
```

Share codec implementations across byte arrays and streams; avoid forced full buffering. Consider gzip/zlib and perhaps ZIP archives as a **separate archive API** (files/entries, not just one compressed blob). Bounded decompression and checked output sizes are especially important with network inputs.

## 15. `log`: simple on the surface, efficient underneath

```void
use log

log.info("Server started")
log.debug("Player position: {position}")
log.warn("Connection is slow")
log.error("Could not load asset")
```

Discussed Microsoft's source-generated/structured logging technique for avoiding unnecessary formatting and allocations when a level is disabled. VOID could eventually use typed fields or deferred formatting, but **do not introduce an entirely new compiler logging framework without measured need**. Make the common call simple. Distinguish `log` application logging from `debug.trace` development diagnostics.

## 16. `process` and separate `args` parsing

Boss corrected an earlier suggestion of requiring `main(args: process.arguments)`: **normal `[string]` parameters are preferred**. Boss reports that `main` already supports parameters and optional return values (and a no-return `main`); verify the exact current source/binding rules instead of relying on an older zero-argument-only Bible snapshot.

```void
use args as cli

func main(raw: [string]):
    if cli.parse(raw) is Ok(var options):
        if options.get<i32>("port") is Ok(var port):
            print(port)
        if options.has("verbose"):
            print("Verbose")
```

The `args` module optionally parses `--flag`, `--tag=value`, `--tag value`, `-v`, positional arguments, and `--` terminating option parsing. Do not guess whether a following positional value belongs to a flag: explicit option descriptions/schemas may be needed. The developer must retain raw arguments for custom parsing. Separately, `process` handles subprocesses, environment variables, exit status, stdout/stderr and program execution (prefer argument-vector APIs over shell interpolation). No special runtime `process.arguments` type required for `main`.

## 17. `crypto` and `checksum`: different safety purposes

```void
use crypto
use checksum

var digest = crypto.sha256(bytes)
var secure_data = crypto.random_bytes(32)?
var crc = checksum.crc32(bytes)
var file_crc = checksum.crc32_file("save.dat")?
```

MD5 may be exposed **for legacy compatibility only**, not as a secure hash and not confused with CRC. CRC32 detects accidental corruption, not adversarial tampering. `crypto` randomness must be distinct from repeatable/gameplay `random`. Think about stream/file hashing without mandatory full-buffer reads.

## 18. `testing` and `bench`: proposed declaration ergonomics and honest profiling

Boss especially likes an opt-in declaration form, exact word still open:

```void
use testing

test func inventory_push():
    var items = [10, 20]
    items.push(30)
    testing.equal(items.count, 3)

bench func inventory_growth():
    var items: [i32] = []
    for i in 0..<1000:
        items.push(i)
```

Candidate commands are `voidc test` and `voidc bench`, using compiler-owned discovery, no user-visible runtime reflection requirement. Benchmarks should report more than elapsed time where measurable: CPU hot spots, allocations/bytes, GC cycles/pauses, peak memory, contention, and selected collision statistics. These measurements require optional instrumentation and must distinguish actual observed counts from unavailable metrics; detailed profiling incurs overhead. Never execute test/benchmark bodies as ordinary production startup or silently impose profiler overhead in release apps. Preserve normal test assertion errors with VOID source locations.

## 19. `regex`: advanced matching without replacing ordinary text methods

```void
use regex
var pattern = regex.compile("[0-9]+")?
var matched = pattern.is_match("Player123")
```

Provide common match/search/replacement/capture functions with clear Unicode/error/resource behavior. Ordinary `.contains()`, `.split()` and `.replace()` should never require regex.

## 20. `sync`: scoped synchronization with folder submodules

Boss likes a `sync/` folder containing `sync.void` and distinct files (`guard.void`, `mutex.void`, `atomic.void`, potentially `semaphore.void`). Expose easy locking using existing `with` cleanup:

```void
use sync
var mutex = sync.mutex()
with mutex.lock():
    score += 10
```

The guard is the acquired lock's scoped ownership, **not** an automatic global variable. `use sync/guard` can expose specialized functionality if it earns a purpose. Explore atomics and semaphores without proliferating wrappers. A regular synchronous lock is **not** automatically safe to retain across `await`; separate async-aware primitives require precise semantics. Existing worker/GC transfer restrictions remain intact.

## 21. `tasks`, `time` and explicit async method names

VOID already has native `async`, `await`, structured groups/cancellation and some `time` APIs under Stage 10. Reuse that, rather than proposing another task execution system. Possible convenience:

```void
use tasks
use time
await tasks.delay(time.milliseconds(500))
```

Boss likes explicit synchronous/asynchronous names throughout relevant libraries: `get`/`get_async`, `read_text`/`read_text_async`, `write_bytes`/`write_bytes_async`, etc. These clarify blocking vs suspension. Cancellation, awaiting, cleanup and source effects must follow the existing runtime, and an `_async` method should not merely start a native thread for every call.

## 22. `channel`: **Boss's separate pub/sub router idea**, not a worker queue

Boss explicitly clarified: by `channel`, they meant an **independently instantiated publish/subscribe router**, not an inherently global channel and not necessarily a competing-consumer task queue. It may be passed around, stored in a runtime global, or private to a subsystem:

```void
use channel
var game_bus = channel.router()

game_bus.subscribe("player/damaged", on_damage)
game_bus.emit("player/damaged", 25)
game_bus.unsubscribe("player/damaged", on_damage)
```

Existing VOID `event` is still important and should stay: typed event members, owner-only `.emit`, ordered synchronous subscriptions, removal and lifetime rules. A channel/router would organize **topics and subscribers across systems** and should not silently replace native `event` or blur with a queue/channel for worker message transfer. Before approving such a library, challenge whether it adds sufficient value, whether string topic/payload typing is safe, and how subscription lifetime, reentrancy, threading, leaks and ownership work. **Global status is a property of the variable holding the router, not of `channel` itself.**

## 23. `debug`: keep asserts and traces; defer programmatic traps

```void
use debug

debug.assert(health >= 0, "Health cannot be negative")
debug.trace("Player updated")
```

Boss wants proper editor line/conditional breakpoints in VS Code, Neovim and Vim. `debug.breakpoint()` was discussed and **deferred**: no need for source-level breakpoint traps if editor debugger integration covers ordinary use, and native trap behavior without a debugger is problematic. Debugger execution control may need DAP; the existing LSP alone does not supply it. No claim that the editors have this debugging capability yet.

## 24. Lower-priority ideas, **not** agreed contracts

`database`/SQLite, `url` parsing/query encoding and portable `system` hardware statistics were mentioned as possible future areas, but have **not** been designed or accepted here. Do not roll them into an authorized stdlib milestone automatically. General reflection, `Activator.CreateInstance` equivalents and runtime type-tree discovery were expressly **withdrawn by Boss**; do not add them to the plan or re-propose them absent a new request.

---

# 25. Main language proposal: file-level runtime `var` in **any** `.void` file

**This is the priority design issue for Wrench review.** Boss clarified it repeatedly. It is deliberately simple for the programmer:

> **`var` inside a function/loop/conditional belongs to that lexical scope. `var` declared at outermost file level, outside those blocks, is one shared runtime global. No `global` keyword. It applies to ANY `.void` file, not only `main.void`.**

The explicit distinction is **outermost file-level declaration**, not literally “any line that isn't textually inside `func`”: a `var` in a top-level `if` or `for` body still belongs to that block, not the file scope. A `var` field declared inside a struct is also governed by the existing field rules, not global-variable rules. Boss wants the same familiar `var` spelling and normal lexical scoping, not a new declaration category exposed to developers.

### 25.1 Expected lexical behavior

```void
# game_state.void
var score: i32             # File-level global, default 0 if type permits
var player_name = "Hero"   # File-level global

func update():
    var bonus = 10         # Local to update(), visible in nested blocks

    for item in items:
        var item_data = inspect(item)  # Visible within this loop

        if item_data == null:
            var is_dead = true        # Only in this if branch
        else:
            is_dead = false           # ERROR: sibling branch cannot see it

        bonus += 1                    # Valid: enclosing function local

    # item_data and item unavailable here
    score += bonus                    # Valid: global

# bonus unavailable at file level
```

If both branches need the same local, the developer declares it in their common enclosing scope and assigns within the branches. Existing no-shadowing rules remain; no `global`/`nonlocal` keyword should become necessary.

### 25.2 Cross-file shared game assets (Boss's motivating use case)

```void
# assets.void
var hero_texture: texture? = null
var assets_loaded = false

func boot():
    hero_texture = load_texture("hero.png")
    assets_loaded = true

func shutdown():
    if hero_texture is Ok(var tex):
        tex.deinit()
        hero_texture = null
    assets_loaded = false
```

```void
# main.void
use "./assets.void" as assets

func main():
    assets.boot()                     # Load once
    # Game update/draw can reuse the same shared asset
    assets.shutdown()                 # Explicit external resource release
```

`texture`, `load_texture` and related calls are placeholders; this is not a Raylib binding contract. Ordinary managed/global roots retain shared objects, while native textures/files/sockets still require deterministic cleanup. `var hero_texture: texture? = null` starts empty and later holds one reusable reference, exactly the scenario Boss described.

### 25.3 `const` must NOT be changed to fake mutable globals

- Current file/module `const` is a **compile-time pure constant** with established project-root visibility and dependency checks.
- Current local `const` creates a runtime readonly binding/view, including deep readonly constraints for managed references.
- Neither should be silently redefined to mutate contained objects. **`var` is the appropriate mechanism for mutable runtime shared state**.

### 25.4 Existing compiler/documentation boundary (critical)

At least the current published contracts explicitly say runtime module globals do **not** exist yet:

- `docs/VALUE_SEMANTICS.md`: runtime module variables remain unsupported; existing root constants are pure and project-wide.
- `docs/STAGE8_APPLICATION_NATIVE.md`: executable top-level statements execute in a compiler-created body; top-level `var` values are currently **locals of that body**, not visible to ordinary functions.
- `docs/STAGE9_RESEARCH.md`: runtime module globals and imported scene instances remain open; imported executable statements must not run automatically.
- The uploaded `LANGUAGE_BIBLE.md` preserves file-level mutable globals as unsettled and separates them from constant behavior.

Please inspect the current source/worktree before presenting any of this as a verified current deficiency: Boss says some application-entry/`main` functionality has advanced beyond older snapshots. Do **not** say “`var` already works globally” merely because the syntax exists at selected-entry top level.

### 25.5 What Wrench must decide/reconcile beneath the simple surface

1. **Definition of file-level declaration:** An outermost `var` should become shared storage in any `.void` module. Top-level `if`/`for` still follow block scope. Existing declaration-only modules and selected-entry script/top-level execution must keep clear, noncontradictory meaning. This is a semantic change for top-level entry `var` previously local to a synthetic function; check compatibility and tests.
2. **Initialization:** Global defaults and initializers run exactly once per application/module identity, not once per import or function call. Define ordering among module dependencies, eager versus lazy initialization, cycle diagnostics, initializer failure cleanup and whether effects are permitted. Imported arbitrary executable statements must NOT start running just because a global exists.
3. **Names and visibility:** Globals are shared *runtime storage*, but should not necessarily be implicitly unqualified in every file. Preserve `use` resolution, private/internal/public access and qualified aliases. The selected entry's root constants are a special implicit project lookup today; decide explicitly whether root runtime globals follow that precedent or use normal module imports. Multiple modules defining `score` must not silently collide.
4. **Type/default rules:** Reuse `var` defaults where safe (`i32` gets zero, nullable gets `null`); no uninitialized garbage or implicit construction of required nonnullable managed references. Do not invent another globals-only type system.
5. **GC and ownership:** Precisely root all global managed/string/array/dictionary/interface content across function calls, JIT/AOT and safepoints. A global may keep a resource wrapper reachable, but GC does not automatically perform its native `deinit`. Preserve explicit lifetime/cleanup and no resurrected readonly authority.
6. **Concurrency:** A mutable global is not automatically atomic or safe to share across workers. Preserve existing worker transfer/ownership restrictions; where sharing is legal, require synchronization. Don't silently broaden Stage 6 parallel sharing rules.
7. **Hot reload:** Keep shared global identity and state across compatible function-body replacements; changes to global declarations, initializer behavior, type/layout or dependency graph likely require an explicit compatibility/restart policy. No silent re-execution or lost assets.
8. **Tooling and correctness:** `voidls`/editor names, hover, completion, rename and diagnostics should use the same scope graph. Test cross-file lookup, initialization once, aliasing, cycles, private access, `const` stability, GC retention/cleanup, no-shadowing, JIT/O2/AOT, and existing application top-level scripts. Focused tests during development; native Windows/macOS evidence only when properly run.

**The developer-visible rule remains tiny; the underlying runtime obligations are real.** This should be treated as one coherent, owner-approved language change if Boss authorizes it later, not a trivial parser tweak or an excuse for a new global-state framework.

---

## 26. Requested Sacred Wrench review and deliverable

Please independently assess the entire set **without implementing it yet**. In particular:

1. **Check the Bible and current source/Stage 10 record** to distinguish current features from proposals and identify conflicts. Remember Boss's later report that `main` can take parameters and may omit its return type.
2. **Evaluate each module for simplicity, cohesion and duplication.** Challenge unnecessary aliases, uncertain error handling and excessive generic/reflection/runtime requirements. Preserve manual bitwise, binary and raw-args control.
3. **Give special attention to file-level `var`**: selected-entry script compatibility, safe once-only module initialization, cross-file visibility, GC roots, concurrency, native resource cleanup and hot reload. Propose the simplest correct semantic contract, with any genuine owner decisions called out separately.
4. **Classify** proposed API names as illustrative unless Boss has explicitly fixed them, and mark major open questions (for example enum flags, floating range inclusivity, Pascal prefix format, callback sorting, typed JSON errors, channel topic typing).
5. Recommend a **bounded design/implementation order only if requested**, taking into account that Stage 10 language completeness is not yet locked. No commits, branches, pushes, hosted CI or root README edits. If this is worth preserving, prepare a proposed documentation update under `docs/` for Boss review, without silently turning discussion into implemented fact.

**North star:** Strong capabilities; familiar small syntax; consistent semantics; zero unnecessary developer ceremony. **Strong bolts. No ghost bolts. Three Clankers. One VOID.**
