# VOID 2.0: Language Bible and Compiler Architecture

**Status:** Design reference, not an implemented language specification  
**Design snapshot:** 10 October 2026  
**Purpose:** Preserve the entire VOID 2.0 design discussion so it can be continued accurately in a new chat or used as the foundation for implementation milestones.  
**Relationship to VOIDC 1.0:** VOID 2.0 is a *new* language design. VOIDC 1.0 pursued C# semantics through a C backend; VOID 2.0 is free to define smaller, independent semantics and is planned around LLVM native code generation.

> **Primary design rule:** Make programming easy, productive, fast, and fun. The compiler and runtime should handle complexity so application developers do not have to.

> **Important:** Code throughout this document is **proposed VOID 2.0 syntax**, not executable code. APIs such as `graphics`, `files`, `tasks`, and `sdl` illustrate planned interfaces, not existing library implementations. Where a detail is unsettled, the document labels it explicitly instead of inventing a decision.

---

## Table of contents

- [Part I. Product goals and non-goals](#part-i-product-goals-and-non-goals)
- [Part II. Syntax and basic types](#part-ii-syntax-and-basic-types)
- [Part III. Modules, names, types, and object-oriented code](#part-iii-modules-names-types-and-object-oriented-code)
- [Part IV. Control flow, patterns, errors, and collections](#part-iv-control-flow-patterns-errors-and-collections)
- [Part V. Memory, execution, concurrency, and native interoperability](#part-v-memory-execution-concurrency-and-native-interoperability)
- [Part VI. Tooling, documentation, and diagnostics](#part-vi-tooling-documentation-and-diagnostics)
- [Part VII. Compiler architecture and LLVM strategy](#part-vii-compiler-architecture-and-llvm-strategy)
- [Part VIII. Implementation roadmap, prototypes, and audits](#part-viii-implementation-roadmap-prototypes-and-audits)
- [Part IX. Complete worked examples](#part-ix-complete-worked-examples)
- [Part X. Decision register and outstanding questions](#part-x-decision-register-and-outstanding-questions)
- [Part XI. Instructions for future implementation sessions](#part-xi-suggested-rules-for-any-future-assistant-or-implementation-session)

---

## 0. How to use this bible

This file is both a design contract and a future-conversation handoff.

- **AGREED:** Explicitly accepted by the designer during discussion. Treat as intended language behavior unless deliberately revised.
- **RECOMMENDED:** Architecture or detailed behavior proposed and accepted in broad terms, but still needs proof or precise specification.
- **OPEN:** Unresolved edge case, API spelling, or implementation tradeoff. Do not silently promote it to a rule.
- **EXAMPLE:** Illustrative pseudocode in proposed VOID syntax. It must not be represented as implemented behavior.

If an implementation exposes a contradiction between agreed features, prefer a simple, consistent rule and bring the conflict up explicitly before locking it in. Do not quietly introduce special cases.

### 0.1 Quick handoff for a fresh conversation

VOID 2.0 is a proposed, compact, statically typed, garbage-collected, natively compiled language drawing ideas from Python, Go, Lua, PICO-8, GDScript, and useful C# features. It is **not** intended to reproduce C# or become an Odin clone. The developer wants all identifiers in lowercase `snake_case` (with the accepted special patterns `Ok`/`Err`), indentation-based syntax, `func`, receiver methods, `struct` reference objects and `value struct` value types, automatic type inference, simple `var`/readonly `const`, explicit `private`/`internal` and public-by-default, optional properties, method `init()`/`deinit()`, `call()`-callable structs, closures and events, `if`/`else if`/`match`/`is`, `Ok`/`Err`/`result`, `for` for all loops (iteration, Boolean conditions, and unbounded repetition), optional index/key, reverse iteration, `defer` with optional `{ ... }` block, arrays `[]` (growable or fixed-size) and dictionaries `{}`, UTF-8 strings with automatic interpolation, native C libraries through safe bindings, no exposed pointers in ordinary code, and excellent IDE diagnostics. LLVM AOT release builds are planned; LLVM ORC is the leading design for development-mode hot reload of compatible function bodies without restarting. GC is planned as precise, non-moving tracing, initially stop-the-world with thread-local small-object allocation and carefully chosen safepoints. **Compiler/runtime complexity must never be pushed onto ordinary application developers.**

### 0.2 Not yet implemented

This document records design decisions and a proposal for implementation. It does **not** claim that LLVM code generation, a collector, a language server, hot reload, a package registry, or any of the syntax shown here already exist for VOID 2.0.

---

# Part I. Product goals and non-goals

## 1. Core principles [AGREED]

1. **Ease of use first.** Simple examples should require very little code; experienced developers should retain powerful types and predictable behavior.
2. **One consistent set of rules.** Borrow good ideas from other languages without blindly copying their quirks or adding several spellings for the same operation.
3. **Native speed.** Aim toward Go-like performance, with efficient LLVM-generated machine code, allocation avoidance, fast collections, and GC performance measured with real applications.
4. **Automatic memory management.** Ordinary developers should not deal with raw pointers, manual frees, `unsafe`, stack/heap placement, or GC scheduling.
5. **Native interoperability.** C libraries must be usable through typed `.void` wrappers and trusted bindings without forcing users to learn C or deal with separate `cstring` values.
6. **Live development.** Compatible function implementation changes apply automatically on save. A running program is never forcibly restarted or repeatedly nagged.
7. **A genuinely useful editor.** Autocomplete, documentation tooltips, parameter help, package discovery, source navigation, and understandable diagnostics are core goals.
8. **Portable behavior.** The same VOID code should behave consistently across Linux, Windows, and macOS where its dependencies support those platforms.
9. **KISS and DRY.** Keep compiler responsibilities well separated; no unnecessary duplicate frontends, execution engines, or monolithic runtime feature systems.
10. **No backend leaks.** Ordinary compile/runtime errors should show `.void` source, not C, LLVM IR, mangled symbols, or linker noise.

## 2. Influences, with a distinct VOID identity

| Influence | Ideas intentionally borrowed | Ideas not automatically copied |
|---|---|---|
| Python | Readable indentation, low boilerplate, compact local code | Fully dynamic typing, implicit truthiness, silent typo-declarations |
| Go | `struct`, receiver `func`, explicit errors, simple concurrency, `defer` | Its complete package/namespace system, pointer syntax, boilerplate error checks |
| Lua | Concise table/dictionary literals and data-oriented convenience | Using one table representation for *every* type and object |
| PICO-8 | Tiny game-friendly APIs, rapid creative iteration | Hard limits on program size or a mandatory fantasy-console runtime |
| GDScript | Clean object/property syntax, accessible editor experience | Forced engine-specific scope or runtime |
| C# | IntelliSense quality, formatting, useful pattern matching, strong diagnostics | CLR dependency, large language surface, `this`, `new`, `out`, verbose property declarations |

## 3. Deliberately excluded or unnecessary features [AGREED]

- No traditional C-style `for (init; test; step)` loop.
- No `this`: receiver names handle instance access.
- No `new`: construct with `player()`.
- No `void` return annotation: omit the return type for no result.
- No `namespace` declaration: VFS paths/folders establish internal module identity.
- No required `out`: use multiple returns, `result`, or pattern bindings.
- No `switch`/`case`: use `match`.
- No `nil`: use `null`.
- No separate user-facing `cstring` type.
- No `unsafe` or raw pointer arithmetic in ordinary VOID application code.
- No mandatory `delegate` keyword: functions are values.
- No compiler-special stack and queue types. Optional library implementations are enough.
- No built-in manual GC controls required by developers.
- No automatic program restart following an incompatible hot-reload edit.
- No C-style `printf` format-string placeholders in regular VOID printing.
- Do not rush user-defined implicit conversion operators, exotic concurrency models, or complex inheritance machinery without a demonstrated need.

---

# Part II. Syntax and basic types

## 4. General syntax [AGREED]

- Indentation introduces most blocks; use a trailing `:`.
- `#` starts a normal comment; `##` introduces declaration documentation.
- No statement-ending semicolons in normal code.
- Naming: lowercase `snake_case` for variables, functions, methods, structs, interfaces, enums, properties, fields, and type parameters.
- The special enum-like result constructors/patterns `Ok(...)` and `Err(...)` retain those spellings because the designer explicitly liked them. The canonical result type is shown here as lowercase `result<t, error>`.
- Boolean operators are `and`, `or`, `not`, with short circuiting. `&&`, `||`, and unary `!` are not part of the normal grammar.
- Ordinary comparisons use `==`, `!=`, `<`, `<=`, `>`, `>=`.
- Conditions require Boolean values, not Python-like implicit truthiness.

```python
const max_health = 100

struct player:
    name: string
    health: i32 = max_health

func main():
    var hero = player(name = "Hero")

    if hero.health > 0 and not hero.name.empty:
        print("Welcome, {hero.name}!")
    else:
        print("Cannot start")
```

### 4.1 Identifier and source details [OPEN]

Exact identifier Unicode rules, tab-versus-space policy, allowed leading underscores, formatter behavior, and detailed token grammar need formalization. The standard style remains four-space indentation and lowercase snake case.

## 5. Variables and readonly constants [AGREED]

`var` creates a mutable variable. `const` creates a readonly binding; assigning a constant's value to a mutable variable does **not** make that variable constant. `=` on an existing variable performs assignment. Redeclaring a visible variable with `var` is a **compile error**.

```python
const max_health = 100

func main():
    var health = max_health
    health -= 25
    health = max_health   # Valid
    # max_health = 200   # Compile error

    if health > 50:
        health = 25       # Valid assignment
        # var health = 25 # Compile error: shadowing

    print(health)         # 25
```

`const` may also appear in functions and methods. An immutable collection cannot be mutated through a `const` binding:

```python
func test():
    const scores: [i32, 3] = [10, 20, 30]
    # scores[0] = 50     # Compile error

    const items = ["sword", "potion"]
    # items.push("bow") # Compile error
```

**Important semantics to specify precisely [OPEN]:** The user wants `const` to mean readonly *data*. A readonly access path must reject field/element mutations and mutating methods. A referenced mutable object may still change through another mutable alias unless the language adds stronger globally immutable/frozen-object semantics. Also distinguish compile-time-evaluable constants, used for array sizes, from readonly runtime object values. Any source change to a `const` requires manual restart in development mode.

## 6. Numeric types and literals [AGREED]

Exact-width types are available: `i8`, `i16`, `i32`, `i64`, unsigned counterparts, `f32`, `f64`, and `bool`. The precise list of platform-sized integer aliases remains open. Unannotated small integral literals normally infer `i32`; unannotated floating literals normally infer `f32`, reflecting the game-development focus.

```python
var health = 100          # i32
var speed = 0.5           # f32 by default
var gravity = 9.81f       # explicit f32
var precision = 9.81d     # explicit f64

var exact_speed: f32 = 100  # Allowed: integral literal adapts
var distance: f64 = 0.1    # Literal interpreted in f64 context
```

Expected types from arguments, fields, and annotations guide unsuffixed literal interpretation:

```python
func set_speed(speed: f32):
    print("Speed: {speed}")

set_speed(100)    # No need to write 100.0
set_speed(0.5)
```

Explicit conversions that may lose precision need an explicit `as`; implicit conversions of existing variables should not silently discard precision. Overflow, integer literal boundaries, `NaN`, and floating-point rounding details require exact specifications and tests.

## 7. Automatic default initialization [AGREED]

Every safe-language field/variable without an initializer must receive a valid default appropriate to its type. No uninitialized garbage values are observable through safe VOID.

| Type | Default |
|---|---|
| Integer | `0` |
| Floating-point | `0.0` |
| `bool` | `false` |
| `string` | `""` |
| Nullable reference/value `t?` | `null` |
| Growable list `[t]` | Empty list, independent per instance |
| Dictionary `{k: v}` | Empty dictionary, independent per instance |
| `value struct` | Fields recursively initialized |
| Non-nullable reference/`func()` | Valid initializer required if no intrinsic safe default |

```python
value struct vector2:
    x: f32
    y: f32

struct player:
    name: string
    health: i32
    alive: bool
    position: vector2
    inventory: [string]
    target: player?

var hero = player()
print(hero.name)         # ""
print(hero.health)       # 0
print(hero.alive)        # false
print(hero.position.x)   # 0.0
print(hero.inventory.count) # 0
```

Default values do **not** determine stack versus heap allocation. Object/value semantics and compiler analysis determine storage.

## 8. Strings, interpolation, and formatting [AGREED]

VOID exposes one public `string` type, with UTF-8 as the chosen internal representation. Every ordinary string supports interpolation automatically, with no `$` or `f` prefix. There is no C-style `%s`/`%f` formatting requirement.

```python
var name = "Hero"
var health = 75.456
var score = 42
var gold = 1250.5

print("Hello, {name}!")
print("Next score: {score + 1}")
print("Health: {health:F2}")   # 75.46
print("Score: {score:D5}")     # 00042
print("Score: {score:0000}")   # 0042
print("Gold: {gold:N2}")       # 1,250.50 with invariant formatting
print("Alive: {health > 0}")
print("Literal braces: {{name}}") # Literal braces: {name}
```

Useful formatting forms include `F2`, `D5`, `N2`, `P1`, `X`, custom zero padding (`0000`), alignment (`<12`, `>12`), and escaped literal braces `{{`/`}}`. Use an invariant, portable default; localization should be explicit.

Multiline strings use triple quotes, with predictable indentation trimming:

```python
var report = """
    ===== PLAYER =====
    Name: {name}
    HP:   {health:F2}
    Gold: {gold:N2}
    ==================
"""
print(report)
```

Custom value display can use a method such as `string()`:

```python
value struct vector2:
    x: f32
    y: f32

func (v vector2) string() -> string:
    return "({v.x}, {v.y})"

print("Position: {vector2(x = 10, y = 20)}")
```

**OPEN:** Exact Unicode character indexing and slicing (bytes, scalar values, grapheme clusters), validation rules for invalid UTF-8, normalization, embedded NUL behavior, and format grammar need formal definitions. The public one-string rule is not open.

## 9. Printing [AGREED]

One simple `print()` handles primitive values, strings, interpolated text, and types supporting string conversion. It adds a newline by default, supports multiple arguments separated by spaces, and allows an optional `end` parameter.

```python
print("Hello")
print(100)
print(true)
print("Score:", 42)
print("Loading", end = "")
print("...", end = "")
print("Done")
```

No separate `printf`/`println` family is required in ordinary VOID.

---

# Part III. Modules, names, types, and object-oriented code

## 10. Project entry point and VFS imports [AGREED]

The project's entry point may be `main.void`, `program.void`, an explicitly configured entry file, or a unique project-root file containing `main()`. The precise discovery priority is recommended and still needs implementation agreement. The entry file defines the project root and provides project-wide constants.

```text
my_game/                    <- virtual project root: /
├── main.void
├── base_unit.void
├── units/
│   ├── player.void
│   └── enemy.void
├── scenes/
│   ├── town.void
│   └── battle.void
└── world/
    └── map.void
```

Paths inside VOID source always use `/`, on Windows, Linux, and macOS. The leading `/` refers to the **virtual project root**, not the OS filesystem root. The physical directory name `my_game` is not included.

```python
use "/units/player.void"        # Root-relative VFS path
use "./enemy.void"              # Relative to current file
use "../base_unit.void"         # Relative to current file
use "math"                     # Standard / installed package
use "/world/map.void" as world # Optional alias
```

Multiple paths to the same physical/module source must resolve to one canonical module identity. Folders provide the compiler's internal qualification, but developers do not write a `namespace` declaration. Directory layout should **not** determine who is allowed to import what. Imports may reference files outside the project root using `../` when explicitly permitted, with stable external-module identities. Case-sensitive module identity across OSes is the proposed policy; filesystem case collisions and symlink behavior need a careful resolver design.

For example, inside `units/player.void`, both `use "/base_unit.void"` and `use "../base_unit.void"` reference the same module. Internally, `units/player.void` might contain `units.player`, but `Player`-style names are **not** the naming convention. A public `player` type can simply be used as `player` following an import. Conflicting imported simple names require qualification or an alias, never silent selection.

### 10.1 Root constants

```python
# main.void
const game_name = "Ghost Hunt"
const tile_size = 16
const max_players = 4

func main():
    print(game_name)
```

```python
# units/player.void
struct player:
    x: i32 = tile_size * 4
    name: string
```

Agreed design: compile-time/project-wide constants from the entry file can be read by other files without repeatedly importing `main.void`. **OPEN:** Exactly which readonly declarations qualify, initialization ordering, how this interacts with external libraries, and how collisions are diagnosed. Avoid creating a hidden cyclic runtime initialization system.

### 10.2 File-level code and scene modules

A `.void` module may contain private functions and top-level instances, rather than requiring every helper function to live inside a struct. The strongest explicitly accepted scene pattern is a **struct holding callback fields wired to private module functions**. An additional proposed feature allowing a module itself to satisfy a scene interface or define `call()` was discussed, but is **not locked as a core language requirement**; do not confuse it with callable structs.

## 11. Visibility and `static` [AGREED]

- **Public by default.** Omit the `public` keyword in ordinary declarations.
- `public` may be written explicitly for documentation or library APIs.
- `private` hides a member from outside its declaring struct; a top-level `private func` is file-private.
- `internal` means visible throughout the current project or compiled library, **not** just the source folder.
- `static` belongs to the type, not an instance; it is *not* a visibility modifier.
- Local variables remain scoped locals regardless of public/internal modifiers.

```python
struct player:
    name: string                     # public
    private health_value: i32 = 100  # struct-private
    internal experience: i32 = 0     # current module/package project boundary
    static player_count: i32 = 0     # shared across instances

func (p player) add_experience(amount: i32):
    p.experience += amount

static func player.create(name: string) -> player:
    return player(name = name)
```

`private func enter_town()` at file scope is callable from the same file, including when attached to a public callback. An imported application can call the public callback but cannot import the private function name directly. IDE completions should show only accessible symbols while source navigation inside the defining file still works normally.

**OPEN:** Exact rules for subclass access to `private` versus `internal` members, and whether a `protected` feature is ever genuinely needed. No `protected` keyword is currently required.

## 12. Ordinary `struct` and `value struct` [AGREED]

The language has **one object-oriented struct family**, with an explicit distinction between reference and value semantics:

| | `struct` | `value struct` |
|---|---|---|
| Semantics | Reference identity | Value/copy semantics |
| Ordinary managed lifetime | GC | Inline/stack/register where possible, GC-tracked when containing references |
| Assignment | Two variables may refer to same object | Assignment copies the value |
| Inheritance | Supported, single base struct | No struct inheritance |
| Interfaces | Supported | Supported |
| Methods/properties | Supported | Supported |
| Appropriate use | Characters, scenes, entities, resources | Position, color, rectangle, compact data |

```python
struct player:
    health: i32 = 100

var hero = player()
var alias = hero
alias.health = 50
print(hero.health)   # 50, same reference
```

```python
value struct vector2:
    x: f32
    y: f32

var a = vector2(x = 10, y = 20)
var b = a
b.x = 50
print(a.x)          # 10, copied value
```

Initializers and default field values don't force an object onto the stack or heap. The compiler may use stack allocation, registers, inline storage, or allocation elimination whenever it preserves semantics. Ordinary reference structs are managed objects conceptually, regardless of storage optimization.

### 12.1 Inheritance and composition

```python
struct base_unit:
    health: i32 = 100
    position: vector2

func (u base_unit) move(dx: f32, dy: f32):
    u.position.x += dx
    u.position.y += dy

struct player extends base_unit:
    name: string = "Hero"

struct enemy extends base_unit:
    damage: i32 = 10
```

Agreed: single struct inheritance and interfaces; composition is encouraged. The exact virtual/override method dispatch policy should be defined before emitting object layouts. A proposed `override` keyword explicitly marks overriding, and `super` invokes a parent implementation:

```python
func (u base_unit) take_damage(amount: i32):
    u.health -= amount

# Proposed override spelling; not yet independently locked
# override func (p player) take_damage(amount: i32):
#     super.take_damage(amount)
#     print("Damage: {amount}")
```

The `super`/`override` mechanism is **RECOMMENDED**, but its full grammar, visibility checks, and virtual dispatch semantics remain **OPEN**.

## 13. Interfaces [AGREED]

Interfaces provide polymorphism without forcing everything into inheritance chains. Structural conformance, as inspired by Go, is the current preferred model: matching method signatures satisfy an interface automatically.

```python
interface damageable:
    func take_damage(amount: i32)

struct enemy:
    health: i32 = 50

func (e enemy) take_damage(amount: i32):
    e.health -= amount

func attack(target: damageable):
    target.take_damage(10)
```

**OPEN:** Whether conformance is strictly structural for all interfaces, how conflicting method names and generic constraints are resolved, and whether an explicit conformance declaration is ever useful. Interface conversion must not silently box small values if a specialized implementation can avoid it.

## 14. Ordinary functions and receiver methods [AGREED]

Every function or method starts with `func`. A method has a receiver `(name type)` before the method name. Parameter annotations are **name-first**: `amount: i32`. Return types use `->`. No `this` or `void` keywords.

```python
func add_numbers(a: i32, b: i32) -> i32:
    return a + b

struct player:
    health: i32 = 100

func (p player) take_damage(amount: i32):
    p.health -= amount

func (p player) is_alive() -> bool:
    return p.health > 0
```

Functions can call other functions and methods in any reachable/imported module. An empty `return` is legal in a no-result function, and such functions may simply reach their end.

### 14.1 Defaults and parameter inference

```python
func move(speed: f32 = 1.0):
    print("Moving at {speed}")

func calculate(value: f64 = 0.5):
    print(value)

# Inferred type f32 from default 0.5
func adjust(speed = 0.5):
    print(speed)
```

A parameter without a type or default is **not** silently dynamic. The current recommendation is to require a type unless a default makes the type unambiguous, and revisit generic inference deliberately. Default values and positional/named calls are supported; argument evaluation order must be specified and preserved across native lowering.

### 14.2 Phase 13 value receiver contract [APPROVED, bounded implementation]

For the numeric/Boolean value-struct subset, methods belong to the nominal type's
owning source module; calls from direct importers and project-root type users
respect public/internal/file-private visibility. Foreign-type extension methods
remain deferred. Receiver mutation is inferred from validated writes and transitive
receiver calls, including cycles. Readonly paths allow only proven non-mutating
methods. Mutating calls require mutable local/parameter storage or a nested field
rooted there; non-mutating calls may use temporaries. Ordinary arguments, assignment
and returns still copy values.

Receiver storage is selected once before arguments; arguments evaluate once left
to right. The method observes argument effects on that storage. Aggregate arguments
are copies taken at their own evaluation point. Changed receiver mutation contracts
require conservative hot-reload restart. See RECEIVER_METHODS.md for the approved
M1 + I1 implementation boundary. No source pointer/reference syntax is added.

## 15. Multiple return values, not first-class tuples [AGREED]

Inspired by Odin: functions can return multiple distinct values without requiring a tuple object. Parentheses group the return type list. Named components improve IntelliSense.

```python
func get_position() -> (x: f32, y: f32):
    return 100, 200

func main():
    var x, y = get_position()
    print("Position: {x}, {y}")
```

Unnamed forms such as `-> (f32, f32)` are valid. Multiple returns are conceptually **not** first-class tuple values; developers use a `struct` or `value struct` for a persistent grouped value.

**OPEN:** Exact ABI lowering, discarding selected returns, named-return metadata, and whether implicit destructuring of non-function values is allowed.

## 16. Constructors: `init()` [AGREED / details OPEN]

Most structs need no explicit constructor. A struct call constructs it with safe defaults and optional named fields. When custom logic is required, define a receiver method named `init`.

```python
struct player:
    name: string
    health: i32 = 100

func (p player) init(name: string):
    p.name = name
    print("Created {name}")

var hero = player("Hero")
```

```python
struct enemy:
    name: string
    health: i32
    damage: i32

func (e enemy) init(name: string, health: i32 = 100, damage: i32 = 10):
    e.name = name
    e.health = health
    e.damage = damage

var boss = enemy("Dragon", health = 500, damage = 50)
```

Agreed intent:

- Fields receive valid defaults before initializer logic uses them.
- `init()` runs once during successful construction and cannot be casually called a second time as an ordinary public method.
- Parent initialization happens once before child-specific initialization. An explicit `super.init(...)` supplies required parent arguments; an available parameterless parent initializer can be automatic.
- Start with **one initializer per struct**, using optional/default parameters rather than a forest of constructor overloads.
- If construction can fail, support `result` and avoid leaking partially initialized objects.

```python
struct player:
    name: string

func (p player) init(name: string) -> result<bool, error>:
    if name == "":
        return Err(error("Name required"))
    p.name = name
    return Ok(true)

match player("Hero"):
    Ok(hero):
        print(hero.name)
    Err(err):
        print("Creation failed: {err}")
```

**OPEN:** Formal rules for constructor result rewriting, errors during parent initialization, ownership of native resources initialized before failure, interaction between named-field construction and custom `init`, and whether `init` should return `result<bool, error>` or a simpler `result<void, error>` equivalent without introducing a `void` keyword. The example is illustrative, not a finalized ABI.

Phase 13 approval selects receiver methods and initializer groundwork (I1), while
public custom `init` execution remains deferred. Existing safe defaults and
named-field construction continue unchanged. No temporary constructor order,
parameter-binding rule or nested initialization behavior is selected by this phase;
those open rules still require approval before implementation.

## 17. Predictable resource cleanup: `deinit()`, `with`, and `defer` [AGREED]

`deinit()` is an optional lifecycle method for releasing native or otherwise externally owned resources; it is **not** a GC destructor. The garbage collector reclaims managed memory independently and does not promise automatic `deinit` calls on collection.

```python
struct texture:
    private handle: texture_handle?

func (t texture) init(path: string):
    t.handle = graphics.load_texture(path)

func (t texture) deinit():
    if t.handle is Ok(handle):
        graphics.unload_texture(handle)
        t.handle = null
```

```python
with texture("hero.png") as sprite:
    graphics.draw(sprite, 100, 100)
# deinit called at scope exit, including early exit
```

```python
func draw_player():
    var sprite = texture("hero.png")
    defer sprite.deinit()
    graphics.draw(sprite, 100, 100)
```

Important rules:

- `with` owns deterministic scope cleanup.
- `defer` schedules cleanup at function exit.
- Defers run in reverse registration order, including on `return`, `?` propagation, and language-managed unwind where safe.
- `deinit` should be effectively one-shot; duplicate native release is not permitted.
- Ordinary `value struct` copying must not accidentally duplicate owned native-resource release obligations. Restrict such ownership until explicit rules are proven.
- Async shutdown/waiting belongs in an async `close()`-style method, then `deinit()` releases resources synchronously.

## 18. Properties and accessors [AGREED]

Ordinary fields need no accessors. A `property` adds getter/setter logic with receiver syntax, using normal field-like access at the call site. Omitting `set` makes the property readonly.

```python
struct player:
    private health_value: i32 = 100
    name: string

property (p player) health: i32:
    get:
        return p.health_value

    set(value):
        p.health_value = clamp(value, 0, 100)

var hero = player()
hero.health = 150
print(hero.health)  # 100
```

```python
struct player:
    health: i32 = 75
    max_health: i32 = 100

property (p player) health_percent: f32:
    get:
        return p.health / p.max_health * 100.0
```

Property getter/setter **body** changes should be hot-reloadable; changing property signature or backing-field layout requires manual restart. Accessors use the same source documentation and IntelliSense metadata as methods. Automatic C#-style properties such as `get; private set;` are not planned unless a real need emerges.

## 19. Static fields and methods [AGREED / method syntax RECOMMENDED]

`static` marks type-level state/behavior and can combine with access modifiers. An instance is not needed to access them.

```python
struct game_settings:
    static max_players: i32 = 4
    private static debug_mode = false

static func game_settings.reset():
    game_settings.debug_mode = false
```

Agreed concept: shared type state and functions. Proposed spelling: `static func type_name.method_name(...)`. Exact syntax for type-qualified declarations outside the struct and initialization/thread safety remains to be formalized.

## 20. Callable structs and function-valued members [AGREED]

A struct defining a suitable `call()` method can be invoked using `object(...)`, like a function, without `extends func()`. Such objects can satisfy a compatible `func(...)` callback type.

```python
struct multiplier:
    amount: i32 = 2

func (m multiplier) call(value: i32) -> i32:
    return value * m.amount

var double = multiplier()
print(double(10))      # 20
```

The callable method can delegate to private helpers in the same file:

```python
func (m multiplier) call(value: i32) -> i32:
    return multiply_value(value, m.amount)

private func multiply_value(value: i32, amount: i32) -> i32:
    return value * amount
```

### 20.1 The explicitly wanted scene pattern

```python
# scenes/town.void
struct scene:
    on_enter: func()
    on_exit: func()
    on_update: func(f32)

private func enter_town():
    print("Welcome to town!")

private func exit_town():
    print("Leaving town!")

private func update_town(dt: f32):
    print("Updating town: {dt}")

var town = scene(
    on_enter = enter_town,
    on_exit = exit_town,
    on_update = update_town
)
```

```python
# main.void
use "/scenes/town.void" as town_module

func main():
    town_module.town.on_enter()
    town_module.town.on_update(0.5)
    town_module.town.on_exit()
```

The functions passed to callback fields are **references**, not invocations: `enter_town`, not `enter_town()`. This pattern was explicitly reinforced by the designer. For callbacks that might be absent, use a nullable function field such as `on_enter: func()?`; non-nullable `func()` fields need valid initializers.

## 21. Function values, anonymous functions, and closures [AGREED]

A `func` can be stored, passed, returned, and invoked. No `delegate` keyword is needed.

```python
func show_damage(amount: i32):
    print("Damage: {amount}")

var handler = show_damage
handler(25)

var on_click = func():
    print("Button clicked")
```

Function types reuse the same syntax:

```python
func run_action(action: func()):
    action()

func apply_damage(amount: i32, callback: func(i32)):
    callback(amount)
```

Closures capture needed local state and keep it alive under GC:

```python
func make_counter() -> func() -> i32:
    var count = 0

    return func() -> i32:
        count++
        return count

var counter = make_counter()
print(counter())  # 1
print(counter())  # 2
```

Mutable captures are shared with the closure. Value structs copied as ordinary values retain the established copy semantics; precise capture-by-reference rules and hot-reload compatibility for closure environments must be designed.

## 22. Events [AGREED]

`event` explicitly denotes a zero-or-more-subscriber callback container, starting empty without an initializer. Use `+=` to subscribe, `-=` to unsubscribe, and `.emit(...)` to notify. A normal `func()` field is **one** callable, not an event.

```python
struct player:
    health: i32 = 100
    event on_damage: func(i32)

func (p player) take_damage(amount: i32):
    p.health -= amount
    p.on_damage.emit(amount)

private func display_damage(amount: i32):
    print("Took {amount} damage")

var hero = player()
hero.on_damage += display_damage
hero.take_damage(25)
hero.on_damage -= display_damage
```

### 22.1 Three distinct declaration kinds

| Code | Meaning |
|---|---|
| `event damaged: func(i32)` | Empty multi-subscriber event, connects later |
| `var handler: func(i32)?` | Optional single callback, initially `null` |
| `const handler: func(i32) = show_damage` | Immutable reference to one callable |

The earlier idea `const handler: func(i32)` as an automatically created event was **withdrawn** to avoid hidden declaration behavior. Use `event` for that purpose.

Events preserve stable callback identity for unsubscribing and hot reload. Ordinary code may subscribe; ideally only the event owner emits its event. The exact rule for file-level events, anonymous-subscription identity, ordering, re-entrancy, synchronization, and observer lifetime is still **OPEN**. `deinit()` is a natural place to unsubscribe; GC alone does not run deterministic unsubscription.


---

# Part IV. Control flow, patterns, errors, and collections

## 23. `if`, `else if`, and Boolean logic [AGREED]

No parentheses required around conditions. All conditions must have Boolean type. Use words `and`, `or`, `not`, not C/JavaScript logical glyphs.

```python
if player.health <= 0:
    print("Defeated")
else if player.health < 25:
    print("Low health")
else:
    print("Healthy")

if player.is_alive() and not player.poisoned:
    player.move(1, 0)
```

`if` can also be an expression whose chosen branch's final expression is the result:

```python
var damage = if critical:
    20
else:
    10
```

**OPEN:** Exhaustiveness of value-producing `if`, required `else`, compatible result type inference, and detailed multiline grammar. A missing value in a value-producing expression must never yield undefined or uninitialized data.

## 24. `is` patterns and `match` [AGREED]

One pattern system is reused for type tests, value/range checks, struct fields, nullable checks, results, and list/dictionary destructuring.

```python
if entity is enemy(e):
    e.attack()

if player.health is 1..25:
    print("Critical")

if settings is {"sound": true}:
    print("Sound enabled")
```

Pattern bindings such as `e` exist only in the branch where the match succeeds. There is no `out` parameter or implicit mutation of an existing variable.

`match` replaces traditional `switch`:

```python
match enemy:
    enemy(health = 0):
        print("Defeated")
    enemy(health = 1..25):
        print("Almost defeated")
    enemy(aggressive = true):
        print("Attack!")
    _:
        print("Ignore")
```

A `match` can return a value:

```python
var status = match player.health:
    0: "Defeated"
    1..25: "Critical"
    26..50: "Injured"
    _: "Healthy"
```

### 24.1 Incomplete matches: carefully agreed distinction

This was specifically discussed and agreed after revising an earlier suggestion:

| Construct | Compiler diagnostic | If no arm matches at runtime |
|---|---|---|
| `match` used as a statement | Warning for missing known cases | Nothing executes |
| `match` used as an expression | Warning for missing cases | Throws a VOID runtime match error |
| Exhaustive `match` / `_` default | No missing-case warning | Selected arm executes |

**Do not silently make incomplete match expressions return nullable values.** That was an earlier suggestion and was explicitly superseded. The runtime failure needs the same source-first diagnostic and stack-trace experience as other runtime errors.

### 24.2 List, dictionary, and value patterns

```python
var items = ["sword", "shield", "potion"]

if items is [first, second, ..rest]:
    print(first)
    print(second)
```

```python
var settings = {"sound": true, "difficulty": 3}
if settings is {"sound": true}:
    print("Sound is on")
```

These are **RECOMMENDED** features in the agreed pattern family. **OPEN:** Exact destructuring syntax, whether extra dictionary keys are allowed, whether `rest` copies or creates a view, exhaustive analysis when data is dynamic, and the distinction between a value pattern and a binding variable.

## 25. Enums: simple and data-carrying [AGREED]

Enums can represent plain alternatives or tagged variants with attached values. Variant payloads are matched using the same pattern syntax, not a separate switch language.

```python
enum direction:
    up
    down
    left
    right

var facing = direction.up

match facing:
    direction.up:
        print("Up")
    direction.down:
        print("Down")
    _:
        print("Other")
```

```python
enum game_event:
    player_joined(name: string)
    enemy_damaged(amount: i32)
    item_collected(item: string)
    game_over

func handle_event(e: game_event):
    match e:
        game_event.player_joined(name):
            print("Welcome, {name}!")
        game_event.enemy_damaged(amount):
            print("Damage: {amount}")
        game_event.item_collected(item):
            print("Collected {item}")
        game_event.game_over:
            print("Game over")
```

Missing cases warn rather than block compilation for statement matches. Value-producing matches warn at compile time and raise a runtime match error if actually unmatched. The precise enum representation and payload ABI are implementation matters.

## 26. Iteration: `for`, optional index/key, and `reverse` [AGREED]

The collection element is always first; optional metadata comes second. Metadata is the original numeric index for a list and the actual key for a dictionary.

```python
var items = ["sword", "shield", "potion"]

for item in items:
    print(item)

for item, index in items:
    print("{index}: {item}")

for item, index in reverse items:
    print("{index}: {item}")
```

A reverse iteration does **not** reorder the collection or allocate a reversed copy. An index during reverse traversal remains the original index.

```python
var stats = {"health": 100, "damage": 15}

for value, key in stats:
    print("{key}: {value}")

for value, key in reverse stats:
    print("{key}: {value}")
```

Dictionary order is insertion order; reverse traverses the reverse of that order. Updating an existing key leaves its position unchanged, while removing and reinserting places it at the end.

### 26.1 Numeric ranges

Ranges reuse one grammar everywhere:

```python
for i in 0..10:     # 0 through 10 inclusive
    print(i)

for i in 0..<10:    # 0 through 9
    print(i)

for i in 10..0:     # 10 through 0 descending, inclusive
    print(i)

for i in 10..<0:    # 10 through 1, excluding 0
    print(i)
```

Direction follows the endpoint ordering. **OPEN:** Range steps, empty ranges, mixed signedness, non-integral endpoints, and overflow behavior. Plain C-style three-part `for` is intentionally omitted.

## 27. `for`, `break`, `continue`, `return` [AGREED]

VOID uses **one loop keyword: `for`**. The separate `while` loop keyword was removed at the designer's request. Use `for` for Boolean-controlled repetition, iterable/range traversal, and unbounded loops. A `for condition:` condition must be `bool` and is checked before each iteration; `for:` has no condition and repeats until control flow exits it.

```python
func update():
    for player.health > 0:
        player.update()

        if player.paused:
            continue

        if game_over:
            break

func stop_if_needed():
    if paused:
        return
```

```python
# Iterate over items or numeric ranges with the same keyword.
for enemy in enemies:
    enemy.update()

for i in 0..<10:
    print(i)

# Unbounded repetition, exited explicitly.
for:
    update_world()
    if should_exit:
        break
```

- `for condition:`: repeat as long as a Boolean condition remains true.
- `for item in iterable:`: iterate a collection or numeric range (see Section 26).
- `for:`: repeat indefinitely until `break`, `return`, or another exit.
- `break`: exit the innermost loop.
- `continue`: begin the next iteration, rechecking a conditional `for` before entering its body again.
- `return`: exit a no-result function; `return value` returns results.
- `while` is **not** a VOID loop keyword. A `while condition:` statement should produce a clear source diagnostic suggesting `for condition:`.

### 27.1 Assignment operators and increments

`+=`, `-=`, `*=`, `/=`, `%=` are supported. `++` and `--` are supported **only as postfix standalone statements**; there is no prefix form or expression-value result.

```python
var score = 0
score++
score += 25
score--

# var previous = score++  # Compile error
# ++score                  # Compile error
```

This retains convenience without complex C/C++ sequencing semantics.

## 28. `defer` and failure handling [AGREED / failure policy RECOMMENDED]

Both a deferred call and an explicit `{ ... }` block are supported, even though most other blocks use indentation:

```python
func load_game():
    var file = files.open("save.dat")?
    defer file.close()

    defer {
        print("Cleaning up")
        print("Finished")
    }

    return file.read()?
```

Defers are registered when execution reaches them and execute **last registered, first executed**. Within a deferred block, statements execute top to bottom. Defers execute on normal returns, early returns, and `?` error propagation.

A language-managed runtime panic inside a defer should not silently skip remaining deferred operations. The original error remains primary, and cleanup failures appear as additional diagnostics:

```text
Runtime error: Game loading failed
  at load_game() src/main.void:12

Additional failure during defer:
  Cleanup failed
  at load_game() src/main.void:6
```

**Important limitation:** Native process corruption, forced termination, or an irrecoverable machine failure cannot guarantee any cleanup or complete stack trace. **OPEN:** Exact lowering and control-flow grammar for `return`, `break`, or `await` inside deferred blocks, whether defer bodies may return `result`, and aggregation/reporting of multiple cleanup errors.

## 29. `result`, `Ok`, `Err`, `?`, and nullable patterns [AGREED]

Recoverable operations return `result<t, e>`, with `Ok(value)` for success and `Err(error)` for failure. `?` unwraps a successful result or propagates its error (running registered cleanup along the way).

```python
func load_player(path: string) -> result<player, error>:
    var text = files.read_text(path)?
    var hero = parse_player(text)?
    return Ok(hero)

match load_player("save.dat"):
    Ok(hero):
        print("Welcome, {hero.name}!")
    Err(err):
        print("Load failed: {err}")
```

`Ok(value)` also serves as a **type-aware pattern** for the presence of a nullable value. It does not make a nullable value into a `result` behind the scenes.

```python
var maybe_player: player? = find_player("Hero")

if maybe_player is Ok(p):
    p.heal(20)
else:
    print("No player")
```

```python
var maybe_items: [string]? = find_inventory()
if maybe_items is Ok(items) and not items.empty:
    print("Found {items.count} items")
```

An empty but present list matches `Ok(items)`; `Ok` checks *presence*, not collection count or truthiness. `Err(...)` is a result-pattern alternative, not a synonym for `null`. `Ok(null)` could be a legitimate `result<t?, e>` success and does **not** imply an error. Nested null/result patterns may therefore need additional checks.

Use proper error types, including enums with cases such as `not_found`, `permission_denied`, and `invalid_format`, rather than only strings. Unexpected programming failures use a language-managed panic/runtime failure path, separate from normal `result` operations.

## 30. Type casts, checks, and parsing [AGREED]

- `value as type` is an explicit checked conversion; when allowed but invalid at runtime, report a VOID runtime error.
- `value as? type` is a checked, nullable conversion; return `null` on failure.
- `value is type` / `value is type(binding)` checks types and narrows them through the pattern system.
- Parse strings using `parse<t>(text)`, returning a `result`, rather than magically casting strings to numbers.
- Compatible numeric literals adapt to expected parameter/field types.
- No C-style `(int)value` casts and no ordinary memory reinterpretation.

```python
var number = 100
var speed = number as f32
var precise = speed as f64

var unit: base_unit = enemy()
if (unit as? player) is Ok(p):
    print(p.name)
```

```python
var age = "25"

match parse<i32>(age):
    Ok(value):
        print("Age: {value}")
    Err(err):
        print("Invalid age: {err}")

# var bad = age as i32  # Compile error: parsing is not casting
```

Explicit floating-to-integer casts were proposed to truncate toward zero, rejecting out-of-range values, `NaN`, and infinity with either runtime error (`as`) or null (`as?`). Exact conversion tables, rounding rules, interface/reference upcasts, and impossible-cast diagnostics are **OPEN**. User-defined conversion operators are intentionally deferred.

## 31. Generics and constraints [AGREED]

`<>` holds generic parameters and arguments. Generic names are lowercase. No arbitrary fixed maximum number of type parameters; a compiler may still place sensible resource safeguards on pathological specializations.

```python
struct box<t>:
    value: t

var health = box(value = 100)          # infer box<i32>
var label = box(value = "Hero")        # infer box<string>
var explicit = box<f64>(value = 0.5)
```

```python
func first<t>(items: [t]) -> t?:
    if items.empty:
        return null
    return items[0]

var names = ["Alice", "Bob"]
if first(names) is Ok(name):
    print(name)
```

```python
struct pair<a, b>:
    first: a
    second: b

var pair_value = pair(first = 100, second = "Hero")
var nested: [result<player, error>] = []
```

Generic constraints use interface syntax where necessary:

```python
func biggest<t: comparable>(a: t, b: t) -> t:
    if a > b:
        return a
    return b
```

**OPEN:** `comparable` interface contract, generic variance, recursive-instantiation detection, specialization sharing, and resulting code-size tradeoffs. The intended semantics retain useful static typing and avoid unnecessary runtime boxing.

## 32. Lists: growable and fixed-size arrays [AGREED]

Only two built-in collection families are required: lists `[]` and dictionaries `{}`. Both are usable without imports. Lists default to growable; specifying a size makes them fixed-length.

```python
var items = ["sword", "shield", "potion"]
items.push("bow")
items.insert(1, "armor")
items.remove("shield")
items.remove_at(0)

var scores: [i32, 4] = [10, 20, 30, 40]
scores[0] = 100    # Allowed: elements mutable
# scores.push(50)  # Compile error: cannot resize
```

```python
const max_players = 4

func calculate_total(const values: [i32, max_players]) -> i32:
    var total = 0
    for value in values:
        total += value
    return total
```

`const values: [i32, 4]` is a **readonly fixed-array parameter**, passed without an unnecessary full-array copy when a safe borrow is possible. No public pointer syntax is needed. Both mutable and immutable fixed-size arrays can appear inside methods, functions, or structs.

| Syntax | Meaning |
|---|---|
| `[i32]` | Growable, typed list |
| `[i32, 4]` | Fixed size of four, mutable unless readonly |
| `[1, 2, 3]` | Inferred growable list |
| `const x = [1, 2, 3]` | Readonly growable list through `x` |
| `const x: [i32, 3] = [1, 2, 3]` | Readonly fixed-size list |
| `[string]?` | Nullable list reference/value, depending on finalized representation |

Fixed-size value arrays can be stored inline where safe. Growable lists use contiguous storage and amortized constant-time push-at-end, subject to actual allocation/growth strategy.

### 32.1 Stack/queue behavior without special compiler collections

The original stack/queue core-type plan was **superseded**. Use list operations or optional `.void` library implementations:

```python
var history = []
history.push("move")
history.push("attack")
if history.pop() is Ok(action):
    print("Undo: {action}")

var messages = []
messages.push("Player joined")
messages.push("Enemy spawned")
var first = messages.remove_at(0)
```

`remove_at(0)` on a contiguous list normally shifts remaining elements and is O(n); a queue implemented as a circular buffer in a library can avoid that for frequent dequeue workloads. Neither `stack` nor `queue` requires a new language primitive.

**API spellings still to finalize:** Earlier examples used both `add` and `push` for append. The user explicitly liked `push`/`pop`; prefer `push`/`pop` in new examples until an API contract is finalized. Supporting both aliases is not automatically agreed because the language favors one obvious way.

### 32.2 Indexing, safe lookup, slicing

```python
var letters = ["a", "b", "c", "d", "e"]
print(letters[0])     # a
print(letters[-1])    # e

var first = letters[0..<3]  # a, b, c
var middle = letters[1..3]  # b, c, d (inclusive endpoint)
```

The chosen list slicing behavior creates a **new list**, not a view, initially. The direct `[]` index operator raises a VOID runtime error on invalid access. `.get(index)` can instead return `result<t, lookup_error>`:

```python
match letters.get(7):
    Ok(letter):
        print(letter)
    Err(err):
        print("No item: {err}")
```

**OPEN:** Whether fixed-size slicing yields fixed or growable results, precise negative-index bounds, safe resizable-collection borrows, and specialized zero-copy views.

## 33. Dictionaries [AGREED]

Dictionaries infer key/value types when possible; explicit declaration uses `{key_type: value_type}`. Keys have predictable equality and hashing semantics; insertion order is guaranteed.

```python
var stats = {
    "health": 100,
    "damage": 15,
    "speed": 4
}

var scores: {string: i32} = {}
var switches: {bool: string} = {}

stats["health"] = 75
stats["defense"] = 10
stats.remove("speed")

if stats.get("health") is Ok(value):
    print("Health: {value}")

for value, key in stats:
    print("{key}: {value}")
```

Order rules: updating a key doesn't move it; deleting and reinserting a key makes it newest. Reverse iteration traverses the reverse insertion order. `.get()` exposes `Ok`/`Err` so a missing key is distinguishable from a present nullable value that happens to be null.

Earlier examples used Lua-like bare keys `health = 100`, while the later typed-dictionary examples favor explicit string keys `"health": 100`. The key-value literal grammar should choose a canonical behavior; both spellings have not been conclusively unified. **OPEN:** Whether bare identifiers imply string keys, reference local variables as keys, or are disallowed.

### 33.1 Equality and hashing

| Key type | Intended default |
|---|---|
| `string` | Equality/hash by UTF-8 text content under explicitly defined rules |
| Integer/bool/enum | Equality/hash by value |
| `value struct` | Field-based if all relevant fields have compatible equality/hash behavior |
| Ordinary reference `struct` | Identity equality by default; mutable object keys discouraged |

Equal keys must always hash equally. Developers should not have to configure a lookup strategy simply because a key is `string`, `bool`, or a supported value struct. Hash algorithms, collision handling, resizing, and lookup behavior belong in the runtime/library implementation, not application code.

**OPEN:** Hashing floating-point keys (`NaN`, signed zero), mutable value-struct fields after insertion, stable hashes across launches, randomization against hostile input, and opt-in custom key equality. Do not silently promise field-hash behavior when it would violate equality constraints.

## 34. Mutation during iteration [RECOMMENDED / exception OPEN]

Ordinary `for` iteration does not create a copy. Adding/removing entries while iterating generally raises a clear runtime error (or a compile-time error where statically provable), avoiding skipped elements and invalidated iterators. Mutating fields of existing referenced objects is allowed.

```python
for e in enemies:
    e.health -= 10   # Allowed

# Removing while iterating forward is generally rejected:
# for e in enemies:
#     enemies.remove(e)
```

A helper such as `remove_if(predicate)` can remove matching elements safely:

```python
private func is_dead(e: enemy) -> bool:
    return e.health <= 0

enemies.remove_if(is_dead)
```

A proposed special case allows removal of the current element during **reverse** iteration:

```python
for e, index in reverse enemies:
    if e.health <= 0:
        enemies.remove_at(index)
```

This is convenient for games and can be safe under specific contiguous-list rules, but exact iterator-invalidating semantics are **OPEN**. Avoid locking it in until tests prove the contract for duplicates, nested iterators, and concurrent modification.

## 35. Custom iteration protocol [RECOMMENDED]

Custom `.void` collections should be able to participate in `for item` and, when supported, `for item, index`. Reverse iteration can require an explicit reversible-iteration capability. The mechanism might use interfaces or compiler-recognized iteration methods, but **should not hardcode every collection implementation into the compiler**.

```python
var tiles = tile_collection()
for tile, index in tiles:
    draw(tile)
```

The syntax is agreed; the precise `iterator`/`next`/disposal protocol is still **OPEN**.

---

# Part V. Memory, execution, concurrency, and native interoperability

## 36. Memory model and object lifetime [AGREED]

Developers should not decide manually between stack and heap based on whether fields have default values. **Storage placement is an optimization; object semantics are part of the language contract.**

```python
struct player:
    name: string
    friend: player?

var a = player(name = "Alice")
var b = player(name = "Bob")
a.friend = b
b.friend = a
```

A tracing GC can eventually reclaim unreachable cycles. A live closure, array, struct, async frame, event handler, or native callback must keep required managed objects reachable. Value structs can contain references; their reference fields need correct GC metadata even if the value struct is inline.

```python
value struct inventory_entry:
    name: string
    owner: player?
```

The compiler can avoid heap allocation if escape analysis proves nonescaping objects retain their required semantics. This is *not* the same as making ordinary structs value types.

### 36.1 Weak references [RECOMMENDED]

Optional standard-library weak references do not keep their targets alive:

```python
var cached = weak<player>(hero)

if cached.get() is Ok(p):
    print("Still alive: {p.name}")
else:
    print("No longer available")
```

**OPEN:** Weak-reference promotion safety, collection timing, and relationship with native callback registrations.

## 37. Chosen GC direction [RECOMMENDED AND ACCEPTED]

Start with a **precise, non-moving tracing mark-sweep GC**, initially using coordinated stop-the-world collections. Design the compiler/runtime interface to permit concurrent marking, generations, or other later refinements without changing ordinary user code.

| Aspect | Initial direction |
|---|---|
| Reachability | Precise tracing, including nested composite references |
| Movement | Non-moving initially; stable object addresses help FFI |
| Cycles | Collected when unreachable |
| Small allocations | Per-thread caches / thread-local fast paths |
| Heap layout | Reusable pages and size classes |
| Large objects | Separate suitable allocation path |
| Thread participation | Registered threads and cooperative safepoints |
| Pauses | Initially stop-the-world, with actual latency measurements |
| Future collectors | Concurrent and/or generational after evidence |
| Weak refs | Supported through runtime contract |
| Resource destruction | `deinit`, `with`, `defer`, separate from GC |

**Performance priority:** (1) avoid unnecessary allocations; (2) make remaining allocations cheap; (3) measure and optimize GC throughput, memory usage, and tail pause latency. Not every loop statement or arithmetic operation should perform a redundant GC poll. But a long-running thread must still eventually cooperate with collection.

### 37.1 Compiler and collector coordination

The compiler must know precisely which locations can hold managed references, including:

- Object fields, value struct fields, array elements, and dictionary entries.
- Stack and register values at GC safepoints.
- Closure environments and callback captures.
- Suspended async/task frames.
- Thread-local runtime roots, globals, and stable handles held by native code.

LLVM offers GC-related statepoint and stack-map mechanisms, but it does **not** provide VOID's collector algorithm or automatically guarantee all reference-liveness invariants. Verify available LLVM APIs against a chosen, pinned version before implementation. Minimize polling costs without sacrificing correctness. Compiler/runtime access to reference write barriers must be extensible, but only pay for barriers that the chosen GC algorithm actually requires.

### 37.2 Thread-local allocation design [RECOMMENDED]

```text
Thread A  -> thread-local allocation cache -> shared managed heap
Thread B  -> thread-local allocation cache -> shared managed heap
Thread C  -> thread-local allocation cache -> shared managed heap
```

Most small managed objects should allocate without a global lock or OS allocation on every request. Cache exhaustion uses a slower refill path. Reuse storage freed by collection. Large allocations take a separate path. Avoid claiming that a specific TLAB or size-class scheme is faster until measured.

### 37.3 Game-friendly GC behavior [AGREED GOAL]

GC is **fully automatic by default**. Developers need not sprinkle collection calls throughout their update loops or schedule GC themselves. Frame-time consistency matters as much as average throughput. Early benchmarks must include worst-case and percentile pause times, not only allocations per second.

## 38. Resource ownership and `deinit` [AGREED]

Managed memory and native resources are distinct:

- **GC** manages memory backing ordinary VOID objects.
- **`deinit()`** releases external resources at deterministic points.
- **`with`** exits a scope safely and invokes cleanup.
- **`defer`** schedules cleanup for function exit.
- Cleanup should be one-shot; duplicate releases are guarded or rejected.

```python
with graphics.texture("hero.png") as sprite:
    graphics.draw(sprite, 100, 100)
```

Never assume an unreachable wrapper automatically releases a file descriptor or GPU resource immediately. Optional finalization, if ever added, cannot substitute for deterministic teardown of important resources.

## 39. Hot reload: user-visible contract [AGREED]

Development builds should watch source files and automatically apply compatible changes when saved:

```python
func (e enemy) attack(p: player):
    p.take_damage(10)
```

Change the implementation to:

```python
func (e enemy) attack(p: player):
    p.take_damage(25)
```

On successful save/recompilation, future calls use the replacement. Existing objects, scenes, events, and game state remain. There is **no automatic restart** and **no repeated restart confirmation dialog**. Invalid code leaves the previous working implementation running and reports normal source-level diagnostics.

| Source change | Development behavior |
|---|---|
| Ordinary function body | Reload automatically |
| Private function body | Reload automatically |
| Struct method body | Reload automatically |
| Getter/setter body | Reload automatically |
| Event handler body | Reload automatically |
| Callable struct `call()` body | Reload automatically |
| `const` value | Restart required; running old value remains |
| Struct field/type layout | Restart required |
| Method/function signature | Restart required |
| Generic constraint or incompatible specialization | Restart required |
| Closure capture environment layout | Restart required |
| Invalid/incomplete source | Keep last working code active |

An editor can show a quiet, one-time status such as “Restart required: struct layout changed”, but should **never** restart without an explicit developer action. Changes that depend on an incompatible layout or newly changed constants must not be partially reloaded into an older running process.

## 40. Hot reload: internal architecture [RECOMMENDED AND ACCEPTED]

Use LLVM AOT for release and LLVM ORC JIT as the leading design for development. Both share the same compiler frontend, VOID IR, language semantics, and runtime contract.

1. Compiler assigns **stable function identities** independent of current compiled code addresses.
2. Development-mode calls that must be replaceable go through stable dispatch entries or another reload-safe indirection mechanism.
3. Changed function bodies are recompiled and fully validated off to the side.
4. Affected replacements are prepared as one compatible **transaction** when they depend on one another.
5. At a safe coordination point, future calls switch to the new versions.
6. Active calls and suspended async operations continue using code/metadata they still need from earlier versions.
7. Retired code is reclaimed only after the runtime proves no thread, closure, callback trampoline, unwind frame, or suspended task requires it.

### 40.1 Callbacks, events, and native function pointers

A stored callback or event subscription must follow the function's stable identity so hot reloading its body does **not** require re-subscribing it. For callbacks registered with C libraries, stable native trampolines and explicit lifetime management are required. Do not replace a native function pointer while foreign code might be calling it.

### 40.2 Inlining and optimization

Development mode may limit inlining and cross-function optimization at reloadable call boundaries. Release builds can optimize and inline aggressively, and must not pay unnecessary JIT dispatch overhead. Measure the development-mode cost for very frequently called game-update functions rather than assuming it is negligible.

### 40.3 Concurrency and code reclamation

GC and hot reload **share a thread-coordination foundation**, but remain separate subsystems. GC object reachability and executable-code lifetime are separate questions. Old code cannot be unloaded merely because a new dispatch target has been published. Active stack traces, unwind metadata, closures and suspended tasks may still depend on it.

### 40.4 One deliberately restricted first implementation

Start with **function-body replacement only** under identical signatures, compatible reference layouts, and stable captures. Do not attempt struct migration, arbitrary changed-constant propagation, or rewriting suspended async state initially. Transactional replacement should avoid mixed generations among dependent functions.

## 41. Native C interoperability [AGREED]

Normal application code uses safe, documented `.void` APIs. Native representation is hidden behind reusable packages:

```text
VOID application (.void)
          |
          v
VOID wrapper library (.void)
          |
          v
Trusted native binding metadata and ABI bridge
          |
          v
C library (SDL / raylib / SQLite / ...)
```

```python
use "sdl"

func main() -> result<bool, error>:
    var window = sdl.create_window("My Game", 800, 600)?
    defer window.deinit()

    for window.is_open:
        window.update()

    return Ok(true)
```

The application developer sees a normal `string`, not `cstring`; a managed `window` object, not `SDL_Window*`; and a helpful `result`, not a raw C error code.

### 41.1 Native binding authoring [RECOMMENDED]

Trusted bindings can describe opaque C handles and ABI calls. Example, not finalized grammar:

```python
native "sdl3":
    type window_handle: opaque

    extern func create_window(
        title: string,
        width: i32,
        height: i32,
        flags: u64
    ) -> window_handle? = "SDL_CreateWindow"

    extern func destroy_window(
        window: window_handle
    ) = "SDL_DestroyWindow"
```

Real SDL declarations and platform-specific types must be verified when producing actual bindings; the above illustrates the wrapper idea. Do not treat the example as an exact existing SDL declaration. A binding author may need access to lower-level restricted primitives or generated glue, but regular VOID developers should not have to write `unsafe` or pointer arithmetic.

### 41.2 One UTF-8 string across the language [AGREED]

Native metadata describes:

- Encoding required by the C API (UTF-8, UTF-16, platform-dependent, etc.).
- Whether the native function **borrows** a temporary string or **retains** its storage.
- Whether a native returned buffer is borrowed, transferred, or separately freed.
- Which native function releases returned/owned resources.
- Whether embedded NUL is allowed and, if not, what VOID error is produced.

The bridge copies, pins, or retains storage as necessary. Such conversion should be optimized away when a safe direct representation can be used. **No second user-facing string type** is introduced.

### 41.3 Opaque resources, GC, and native callbacks

The GC owns the VOID wrapper; the native library may own the underlying resource until `deinit()`. C callback registrations may retain function pointers and captured VOID references, which must stay alive until unregistered. Calls back into VOID from foreign threads require runtime registration and safe GC state transitions. Incorrect native declarations or buggy C code remain capable of causing process failures, despite safe wrapper APIs.

### 41.4 Packages and cross-platform linking

An import such as `use "sqlite"` should select compatible package/native artifacts for the target OS and CPU, including `.dll`, `.so`, or `.dylib` as appropriate. Static and dynamic linking are both goals where available. A C library unavailable on the desired platform cannot be made portable by syntax alone; the tooling should explain that clearly.

**OPEN:** Package manifest schema, native ownership annotation format, header-import generation, FFI calling conventions, callback exception propagation, dynamic library lifetime, and supported C ABI type mapping. These should be fixed before claiming arbitrary C libraries work automatically.

## 42. Async, tasks, and `spawn` [AGREED]

The syntax should feel like ordinary functions with only necessary async markers. `async func` indicates suspension capability; `await` retrieves the eventual result. Developers do **not** write explicit `task<t>` returns for ordinary async declarations: the compiler creates the task machinery.

```python
async func load_player(name: string) -> player:
    var data = await files.read_text(name)
    return player(name = data)

async func main():
    var hero = await load_player("hero.dat")
    print("Welcome, {hero.name}!")
```

Calling an async function starts/schedules its asynchronous work according to runtime rules and yields a task-like handle. Multiple operations can be in flight:

```python
async func main():
    var player_job = load_player("hero.dat")
    var enemies_job = load_enemies()

    var hero = await player_job
    var enemies = await enemies_job

    print("Loaded {enemies.count} enemies")
```

`spawn` explicitly schedules work, especially a synchronous CPU-bound function, onto the runtime scheduler:

```python
func calculate_path() -> [vector2]:
    return find_path()

async func main():
    var job = spawn calculate_path()
    var path = await job
```

`spawn` must **not** imply creating a dedicated OS thread per call. Most jobs can run through a worker pool. Dedicated threads are available through a library when genuinely needed.

### 42.1 `result` and `await`

```python
async func load_settings() -> result<string, error>:
    var data = (await files.read_text("settings.dat"))?
    return Ok(data)
```

A propagated error still runs registered defers. A task returning `Err(...)` is a normal completed task carrying a failure result, not necessarily a runtime panic. Panics and cancellation have separate rules.

### 42.2 Cancellation [AGREED]

```python
var job = spawn calculate_world()
job.cancel()
```

Cancellation is cooperative, observed at cancellation-aware calls or checkpoints. It must not kill threads or bypass required cleanup. **OPEN:** Cancellation status/result representation, default cancellation propagation across async call graphs, and timing guarantees for long CPU loops.

## 43. Synchronization [AGREED]

Use standard-library `sync` primitives rather than inventing new control-flow keywords:

```python
use "sync"

struct scoreboard:
    private score: i32
    private guard = sync.mutex()

func (s scoreboard) add_score(amount: i32):
    with s.guard.lock():
        s.score += amount

func (s scoreboard) get_score() -> i32:
    with s.guard.lock():
        return s.score
```

Optional `sync.atomic<t>` handles well-defined atomic operations. Ordinary mutex guards are not intended to survive `await`; async-aware synchronization must be a deliberate separate library design. Lock acquisition and release integrate with `with`, `defer`, and cleanup. Diagnostics may report detectable deadlocks, but runtime prevention of all deadlocks is not promised.

## 44. Task groups and scene shutdown [AGREED]

Task lifetimes should be structured rather than silently detached. A task group owns children, waits for them, and supports cancellation. Scene cleanup should stop/join its tasks *before* deinitializing resources.

```python
use "tasks"

private async func load_enemies():
    await load_enemy_data()

private async func load_music():
    await load_audio_data()

async func load_game():
    with tasks.group() as jobs:
        var enemies = jobs.spawn(load_enemies)
        var music = jobs.spawn(load_music)
        await enemies
        await music

    print("Loaded")
```

```python
struct battle_scene:
    private active = true

async func (s battle_scene) close():
    await s.stop_tasks()
    s.deinit()
```

The task group needs async-aware exit behavior and should not secretly block a worker thread while waiting for children in an async context. A child panic cancels unfinished siblings and is reported with additional failures if necessary. Normal `Err` returns are propagated or handled according to the `result` contract. **OPEN:** Exact task-group API signature, owner lifetime, cancellation join policies and explicit detached-task support.

## 45. Async and hot reload together [RECOMMENDED]

A suspended async function retains any managed objects and compiled code needed for correct resumption. If its function body changes during suspension, existing activations may continue with their old compatible version, while newly invoked activations use the new version. Do not attempt to migrate changed async state-machine layouts automatically. Preserve meaningful asynchronous stack traces in VOID source terms.

---

# Part VI. Tooling, documentation, and diagnostics

## 46. First-class docs in `.void` source [AGREED]

Documentation is written for VOID developers, not in C-like ABI language. Ordinary comments use `#`; declaration docs use `##`, including optional parameter descriptions.

```python
## Represents a point in world space.
value struct vector2:
    ## Horizontal coordinate in pixels.
    x: f32

    ## Vertical coordinate in pixels.
    y: f32

## Moves a player to a new location.
##
## position: Destination in world coordinates.
## speed: Units per second; default is 1.0.
func (p player) move_to(position: vector2, speed: f32 = 1.0):
    p.position = position
```

Docs should be attached to compiler symbols (structs, fields, properties, methods, functions, interfaces, variants, generic parameters, and accessible internal/private elements) so the editor and CLI use the same information. XML documentation is not required.

### 46.1 IntelliSense and editor behavior [AGREED]

Typing `player.` should list accessible fields, properties, methods, and docs. Typing `player.position.` should know the nested type; typing `player.move_to(` should show signature help and per-parameter descriptions.

Proposed editor features:

- Context-aware completion and type-aware filtering.
- Hover documentation, parameter help, and generic type information.
- Go to definition, find references, rename across imported VFS paths.
- Diagnostics from the same frontend used to compile.
- Suggested quick fixes when confidence is high.
- Completion even while a file is temporarily syntactically incomplete.
- Source navigation through installed `.void` library implementations.
- Proper filtering by public/private/internal visibility.
- Active hot-reload/restart-needed status without popup nagging.

**Architecture rule:** The language server uses the compiler's parsing, symbol, type-checking, and diagnostics infrastructure; it must not create a second interpretation of the language.

## 47. Library discovery in the extension [AGREED GOAL]

The editor should provide a searchable package catalog with clear README/docs, compatible platform/architecture, version information, install action, and direct source navigation. A successful install enables a simple `use "package"` statement and immediate completion support, where available.

Example discovery scenario:

1. Developer types `use "sdl"` or searches for `sdl` in the extension.
2. The extension identifies whether a compatible package and native dependency exist.
3. It offers a clear install/open documentation action.
4. The package resolver registers the source/native metadata for the current target.
5. LSP completion offers documented VOID wrappers, **not** raw C headers unless explicitly exploring a binding package.

No actual registry or package manifest has been specified yet. Package distribution and trust/security verification are design work, not an assumed solved problem.

## 48. Unified diagnostics [AGREED]

All diagnostics should be easy to understand regardless of experience level: file/line/column, human-readable problem, diagnostic code, source snippet with precise range, expected/received values where useful, and a practical hint. Runtime failures additionally show a VOID-source stack trace.

### 48.1 Compile-time example

```text
src/player.void:12:19: error: Cannot assign a string to i32
    code: VOID2001

  12 | var health: i32 = "hello"
     |                   ^^^^^^^

     Expected: i32
     Received: string

     Hint: Use an integer value, or parse a
           valid numeric string with parse<i32>().
```

### 48.2 Redeclaration/shadowing example

```text
src/main.void:5:13: error: Variable 'health' is already declared
    code: VOID2005

   5 |         var health = 25
     |             ^^^^^^

     Previously declared at src/main.void:2

     Hint: Remove 'var' if you intended
           to update the existing variable.
```

### 48.3 Missing native package example

```text
src/main.void:1:5: error: Native library not found
    code: VOID3001

   1 | use "sdl"
     |     ^^^^^

     Library: SDL3
     Target:  linux-x64

     Problem:
       No compatible SDL3 native library was found.

     Hint:
       Install the missing dependency through
       VOID's extension or package manager.
```

Distinguish nonexistent package, unsupported platform, missing native dependency, wrong architecture, incompatible package version, and actual compiler errors. Never collapse them into a cryptic linker failure.

### 48.4 Runtime error with source stack trace

```text
src/units/player.void:27:26: runtime error: Index out of range
    code: VOIDR1003

  27 | var item = inventory[5]
     |                      ^

     Index: 5
     Valid indices: 0..2

     Hint: Check the collection length before
           accessing the requested index.

Stack trace:
  at player.get_item(index: i32)
     src/units/player.void:27

  at game.update(dt: f32)
     src/main.void:42

  at main()
     src/main.void:10
```

### 48.5 Unmatched expression example

```text
src/main.void:16:18: runtime error: Match expression had no matching case
    code: VOIDR1005

  16 | var label = match facing:
     |             ^^^^^^^^^^^^

     Value: direction.left
     No matching arm or '_' default exists.

     Hint: Add a matching case or '_' fallback.

Stack trace:
  at game.get_direction_label()
     src/main.void:16
  at main()
     src/main.void:6
```

Diagnostic codes above are **illustrative placeholders**, not reserved/implemented IDs. At runtime, report original failure and supplemental defer failures without hiding source locations. Optimized builds should retain or be able to locate enough source information for useful diagnostics; investigate metadata-size tradeoffs. Native corruption may prevent a full stack trace. Optional backend-debug mode can expose LLVM/linker details to compiler developers, but ordinary errors stay in VOID terms.

## 49. Error policy and LSP quick fixes

- Compile errors block invalid source from becoming active code, but do not terminate an already running hot-reload process.
- Warnings are not errors. A non-exhaustive *statement* `match` warns yet compiles; a non-exhaustive *expression* warns and may fail at runtime if unmatched.
- Runtime panics are distinct from recoverable `Err` values.
- Native package resolution failures explain which dependency/target is missing.
- Give suggestions only when grounded; do not offer changes that silently alter developer intent.
- Language server and CLI display identical diagnostic meaning and source spans.
- Use clickable VFS source positions in supported editors, translating them to physical project files under the hood.

---

# Part VII. Compiler architecture and LLVM strategy

## 50. Proposed compiler pipeline [RECOMMENDED]

```text
             VOID source (.void)
                     |
                     v
          Source manager + VFS resolver
                     |
                     v
             Lexer + Parser
                     |
                     v
          AST + symbol declarations
                     |
                     v
     Name resolution + semantic/type checks
                     |
                     v
           VOID typed intermediate IR
              /               \
             v                 v
      LLVM AOT lowering   LLVM ORC development lowering
             |                 |
             v                 v
       Native executable  Running live application
              \               /
               \             /
                Shared VOID runtime
    (GC, allocation, strings, tasks, native ABI,
       runtime diagnostics, code lifetimes)
```

**One frontend; one language meaning.** Both AOT and JIT lowering consume the same validated, typed IR. The language server also reuses the frontend. Debug/source metadata flows through every stage.

### 50.1 Major module responsibilities

| Subsystem | Owns | Must avoid |
|---|---|---|
| Source manager/VFS | Canonical source identity, normalized paths, source spans, import graphs | OS path logic leaking into language semantics |
| Lexer/parser | Indentation and grammar, tolerant parsing | Defining runtime behavior |
| Symbols/semantics | Declarations, generics, type inference, scopes, access, match coverage | Emitting native instructions |
| VOID IR | Typed operations, control flow, managed references, source locations, cleanup edges | Unnecessary duplicate LLVM optimization framework |
| LLVM lowering | LLVM IR and native object/JIT machine code | Independent reinterpretation of language rules |
| Managed runtime | Allocation, GC, roots, threads, synchronization, panic metadata | Reading/parsing `.void` source at normal runtime |
| Standard library | Collections, files, sync APIs, user-facing native wrappers | Compiler-specific hardcoding of routine methods |
| LSP | Completion, hovers, edits, references, diagnostics | Duplicate language compiler |
| Development host | Watch files, incremental builds, reload transaction, code retirement | Restarting without user request |

### 50.2 Internal representation: keep it modest

VOID IR should represent only what LLVM cannot infer from higher-level syntax alone:

- Validated typed expressions and values.
- Explicit branch/control-flow blocks and multiple return values.
- Object/value layouts and field operations.
- Function identity and direct versus virtual/callable dispatch.
- Managed allocation, reference liveness/safepoints, and necessary write barriers.
- Pattern checks, result propagation, deferred cleanup paths.
- Exception/panic unwind semantics that remain valid after lowering.
- Source locations and debug metadata.

**Do not** build a second giant optimizing compiler before LLVM. Perform necessary VOID-specific canonicalization, then let LLVM optimize the machine-level program. Preserve evaluation order and observable side effects precisely.

### 50.3 LLVM as backend, not language definition

LLVM is the leading planned native backend for VOID 2.0. Release mode should generate optimized machine code ahead of time. Development mode should use LLVM ORC for native JIT code and safe replacement. The precise LLVM version, C versus C++ API integration approach, native toolchain/linker requirements, and supported targets remain implementation decisions. Pin a tested version; do not casually track incompatible LLVM APIs.

The exact LLVM binding choices require checking current upstream API/version behavior before implementation. LLVM's GC mechanisms help provide root/reference location data, but don't design and implement VOID's entire tracing collector. ORC's resource tracking helps manage JIT code, but doesn't automatically prove no thread is executing old code.

### 50.4 Alternate backend fallback [REVIEWED, NOT SELECTED]

Source-to-C is a legitimate compiler technique; it was used for VOIDC 1.0. QBE and Cranelift are other native codegen candidates. They may be worth evaluating only if LLVM integration becomes a disproportionate cost. The current design is **LLVM AOT + ORC JIT**, because runtime code replacement is a first-class goal. Do not switch the backend or introduce a second interpreter without a concrete reason.

## 51. Platform independence [AGREED GOAL]

VOID language rules, VFS syntax, numeric operations, string/formatting behavior, GC, result/error handling, and ordinary libraries should be consistent across Windows, Linux, and macOS. Source paths always use `/`. The compiler maps virtual paths to physical OS paths at the filesystem boundary.

Examples:

```text
VOID VFS path: /units/player.void

Linux:   /home/dev/my_game/units/player.void
Windows: C:\Games\my_game\units\player.void
macOS:   /Users/dev/my_game/units/player.void
```

A proposed CLI might look like:

```bash
void run .
void run --live .
void build .
void build --release .
void build --target windows-x64 .
```

These command spellings are proposals, not finalized CLI syntax. Cross-compilation still requires supported target toolchains and compatible native libraries. Detect unsupported combinations early and report them in VOID-friendly diagnostics.

## 52. Crosscutting correctness contracts

These requirements bind all subsystems and should be part of the test design, not postponed until late integration:

1. **Evaluation order:** Named, default, positional, native, async, generic, and constructor arguments evaluate in one well-defined order, with side effects preserved.
2. **Object/reference identity:** Ordinary structs stay reference-semantic; values copy. No optimization changes identity or alias-visible mutations.
3. **Initialization:** Safe code sees no garbage values; per-instance mutable defaults are not accidentally shared.
4. **Failure cleanup:** `?`, early return, panic handling, `with`, and `defer` all preserve required cleanup and original error information.
5. **Concurrency safety:** GC coordination works with native calls, external callback threads, task suspension, and active hot-reload code.
6. **Source diagnostics:** Maintain exact spans and stack-trace mappings across LLVM optimization, generics, closures, and async lowering.
7. **Hot-reload compatibility:** Never install code that assumes a changed struct layout, const value, signature, or incompatible closure/async frame.
8. **Portability:** No source-visible surprises from `\` versus `/`, CRLF versus LF, platform encodings, or native linker diagnostics.
9. **Runtime/library boundary:** Each component has a documented responsibility; adding a collection method or wrapper should not require inventing a new compiler opcode.

---

# Part VIII. Implementation roadmap, prototypes, and audits

## 53. First: architectural proofs before major implementation [RECOMMENDED]

Rather than designing hundreds of milestones and discovering a broken LLVM/GC/hot-reload assumption too late, build four isolated, measurable feasibility demonstrations.

### Proof A: Native LLVM pipeline

**Build:** A tiny compiled program supporting integers, branch, function call, and source locations.  
**Verify:** AOT executable works, named functions are callable, debug metadata maps back to proposed `.void` paths, and error paths are not represented only as LLVM failures.  
**Gate:** Native compile and run on Linux; early Windows run/build check using a reproducible toolchain.

### Proof B: Precise managed references and GC

**Build:** A minimal non-moving collector with accurate roots/field descriptions, allocation, reclamation, and cooperative safepoints.  
**Verify:** Reachable objects survive optimized native calls and loops, unreachable cycles collect, value structs containing references are traced, native temporary handles are safe, and repeated stress tests do not expose stale roots.  
**Gate:** Correctness under optimized builds before allocator micro-optimization.

### Proof C: Running native function replacement

**Build:** Two versions of a function with a stable dispatch identity and safe publication of new code.  
**Verify:** Existing state survives; callbacks keep working; active calls can finish with old code; metadata/code isn't reclaimed prematurely; incompatible changes are rejected without restart.  
**Gate:** Repeated replacement across threads and callbacks without crashes or stale dispatch.

### Proof D: Native C ABI bridge

**Build:** Call a small known C function, pass and receive UTF-8 text through declared ownership rules, and wrap a resource with `deinit()`.  
**Verify:** No developer-facing `cstring`, correct lifetime, no double release, clear error on missing library/architecture, and a C callback can reenter VOID safely under chosen runtime rules.  
**Gate:** Same higher-level source on two operating systems, allowing target-specific native binary packages.

If one proof reveals a constraint, change the implementation approach before building numerous features on top of it. The language contract should remain stable unless the proof reveals a fundamental inconsistency.

## 54. Main implementation stages [RECOMMENDED]

The stages are architectural groupings, **not** preapproved milestone numbers. Some work overlaps, and source metadata plus a minimal language server must start early.

| Stage | Focus | Exit evidence |
|---|---|---|
| 1. Frontend foundation | VFS paths, lexer, indentation parser, AST, symbol tables, source spans, diagnostics, basic type inference | Parse/name/type test suite, cross-file imports, early LSP hovers |
| 2. LLVM native AOT | Scalar types, functions, branches, returns, basic object files, native entry point | Linux/Windows native hello-world and focused ABI tests |
| 3. Managed memory | Reference structs, value structs, object layouts, allocator, precise roots, GC, safepoints | Correct reachability, cycles, escape/storage tests under optimization |
| 4. Core runtime/types | UTF-8 strings, interpolation/formatting, arrays, dictionaries, methods, properties, interfaces | Collection, string, initialization, and object-model integration suite |
| 5. Live code | ORC JIT, watch/compile, dispatch indirection, safe patching, code retirement | Running program survives repeated compatible edits |
| 6. Advanced language | Closures, events, callable structs, generics, `result`, pattern matching, `defer`, fallible init/deinit | Interoperability between GC/lifetime/error features |
| 7. Concurrency | Scheduler, threads, tasks, async, cancellation, mutex/atomic, task groups | Threaded GC tests, cancellation cleanup, async hot-reload stress |
| 8. Tooling/ecosystem | Mature LSP, doc comments, native bindings, package management/discovery, diagnostics | User can install library, navigate source, compile, diagnose and run |
| 9. Optimization/release | Allocation elimination, native specialization, SIMD, GC tuning, release packaging | Real benchmark comparisons, pause percentile results, target-platform acceptance |

**Ordering refinement:** The IR, GC roots, and reloadable-function identity must be *designed* in Stage 1, and the four architectural proofs should occur early enough that later stages depend on demonstrated contracts. Don't wait until Stage 5 to discover reloadable functions need stable identities. Don't wait until Stage 8 to discover the frontend can't support incremental IDE parsing.

### 54.1 What not to do

- Don't finish the entire parser before testing one native LLVM execution path.
- Don't build a second independent type checker for the LSP.
- Don't create a bytecode interpreter solely because live reload sounds like scripting.
- Don't hardcode every list/dictionary method into the compiler.
- Don't let public LLVM API types leak into language symbols or documentation.
- Don't assume non-moving GC eliminates all native lifetime hazards.
- Don't make every method indirect in release builds merely to support development hot reload.
- Don't add complex optimization machinery without measurement.
- Don't postpone Windows testing until another project-ending all-platform audit.

## 55. Suggested milestone discipline [RECOMMENDED]

The previous compiler was developed through focused numbered milestones with strong validation. Reuse the successful process, while avoiding unnecessary repetition of standing rules in every prompt.

Each ordinary milestone should have:

1. One narrow semantic/runtime objective and the precise files/components expected to change.
2. An explicit contract: what works, what intentionally remains unsupported, and what cannot regress.
3. Focused positive/negative tests, including exact diagnostics when relevant.
4. A quick integration smoke suite against already completed compiler/runtime features.
5. A build/test summary, unresolved risks, and source changes actually made.

At boundary milestones, perform a broader audit covering compiler, runtime, allocator, IR, LSP, hot reload, and native bindings as applicable. Prefer one standing design/testing-rules section per milestone block and shorter milestone-specific followups afterward.

### 55.1 Test matrix that follows from this bible

| Dimension | Representative validation |
|---|---|
| Parsing | Indentation, comments/docs, `{}` defer blocks, multiline strings, nested generics |
| Type checking | Numeric literals, optional parameter types, const borrow, inheritance, pattern narrowing |
| Module system | VFS root/relative aliases, canonical identity, external imports, case collisions |
| Control flow | `if` expressions, non-exhaustive match behavior, ranges, reverse iteration, `break`, `continue` |
| Collections | Fixed versus growable, readonly params, `get`, missing keys, hashing/equality, iteration mutation |
| Object model | GC reference structs, copied value structs, `init`, `deinit`, interfaces, override checks |
| Errors | `Ok`/`Err`, `?`, defer ordering, multiple failures, runtime stack traces |
| Callables | Closures, callback identity, events, callable structs, private function references |
| GC | Precise roots, cycles, native retention, threads, async captures, allocator contention |
| Hot reload | Valid body change, invalid syntax, const/layout/signature changes, active calls, callbacks |
| Native ABI | UTF-8, embedded NUL, retained pointers, native returns, destroy once, callback threads |
| Tooling | Partial files, completion, docs, diagnostics identity, rename/refactor across VFS |
| Platforms | Linux, Windows, macOS where supported; debug/release; multiple target architectures |

Use sanitizer/instrumented builds where appropriate, but do not confuse checking the compiler or runtime with checking generated application code. Treat compiler correctness and user-program behavior as two separate test surfaces.

## 56. Performance measurement plan [RECOMMENDED]

Target **Go-like efficiency** as an aspiration, not an unproven guarantee. Compare equivalent workloads and semantics under optimized releases on the same hardware and toolchain configuration. Include optimized C as a lower-level baseline where useful, but do not call it a like-for-like managed-language comparison if it omits bounds checks, GC, or other guarantees.

Measure:

- Scalar arithmetic and loop overhead.
- Allocation rate and small-object allocation latency.
- Reference/value struct access, method dispatch and virtual/interface calls.
- List indexing and growth, dictionary lookup, iterators, reverse iteration.
- String interpolation/formatting and UTF-8 operations.
- GC pauses (median, p95, p99, maximum), throughput, fragmentation, heap overhead.
- Scheduler overhead, sync contention, async suspension/resumption.
- C ABI string/callback overhead.
- Development-mode hot reload dispatch versus optimized release dispatch.
- Compilation latency and total produced executable size.

**Correctness first.** A performance change is acceptable only if it preserves defined language behavior, including errors, GC roots, cleanup, evaluation order, and concurrency. The experience with VOIDC 1.0's redundant safepoints and optimization barriers is a reason to instrument and measure, not a reason to remove GC or make users write unsafe code.

---

# Part IX. Complete worked examples

The examples below intentionally combine features across files. They are **language-design examples**, not currently compiled, and their convenience library functions (`game`, `graphics`, `files`) are illustrative.

## 57. Example project: small game with root constants, scene callbacks, events, and private helpers

```text
little_town/
├── main.void
├── units/
│   └── player.void
├── scenes/
│   └── town.void
└── resources/
    └── saved_game.void
```

### 57.1 `main.void`

```python
use "/units/player.void" as units
use "/scenes/town.void" as town_module

## Constants in the entry file are available across this project.
const game_name = "Little Town"
const max_health = 100
const starting_position = 16

private func show_damage(amount: i32):
    print("Player took {amount} damage!")

func main():
    var hero = units.player(name = "Hero")
    hero.on_damage += show_damage

    print("Welcome to {game_name}!")
    town_module.town.on_enter()
    town_module.town.on_update(0.5)

    hero.take_damage(25)
    print("Remaining health: {hero.health:0000}")

    town_module.town.on_exit()
    hero.on_damage -= show_damage
```

### 57.2 `units/player.void`

```python
## A game character with managed reference semantics.
struct player:
    name: string
    private health_value: i32 = max_health
    position: vector2
    event on_damage: func(i32)

## Protects the player's health range.
property (p player) health: i32:
    get:
        return p.health_value

    set(value):
        p.health_value = clamp(value, 0, max_health)

func (p player) init(name: string):
    p.name = name
    p.position = vector2(x = starting_position, y = starting_position)

func (p player) take_damage(amount: i32):
    p.health -= amount
    p.on_damage.emit(amount)

value struct vector2:
    x: f32
    y: f32
```

### 57.3 `scenes/town.void`

```python
## A scene exposes a few callbacks; internal logic stays file-private.
struct scene:
    on_enter: func()
    on_exit: func()
    on_update: func(f32)

var visits = 0

private func enter_town():
    visits++
    print("Entering town, visit {visits}")

private func exit_town():
    print("Leaving town")

private func update_town(dt: f32):
    print("Updating town by {dt:F2}s")

var town = scene(
    on_enter = enter_town,
    on_exit = exit_town,
    on_update = update_town
)
```

This example illustrates:

- `main.void` project-wide constants.
- Normal `use` paths and aliases.
- Lowercase `snake_case` everywhere.
- `struct` plus `value struct`.
- `init()` and a property with `get`/`set`.
- A public event with a private function subscriber.
- Public scene callback fields wired to private file functions.
- Default float formatting, custom zero padding, and automatic interpolation.
- No `class`, `new`, `this`, or `namespace`.

**Unsettled technical detail:** `town` is a mutable module-level instance, while root constants have special global visibility. The initialization/access rules of ordinary cross-module globals remain to be specified. That does not affect the agreed file-private callback pattern.

## 58. Example: `result` and optional values

```python
enum file_error:
    not_found
    invalid_format
    permission_denied

struct player:
    name: string
    level: i32 = 1

func load_player(path: string) -> result<player, file_error>:
    var text = files.read_text(path)?
    return parse_player_data(text)?

func start_game():
    match load_player("save.dat"):
        Ok(hero):
            print("Welcome back, {hero.name}!")

        Err(file_error.not_found):
            print("Starting a new game")

        Err(err):
            print("Could not load save: {err}")

    var maybe_player: player? = find_online_player("Hero")
    if maybe_player is Ok(found):
        print("Player is online: {found.name}")
```

`Ok(...)` has a type-directed meaning:

- A `result` matches `Ok` when it succeeded.
- A nullable value matches `Ok` when non-null.
- A successful result containing a null value is still a success; inner null checks remain separate.

The placeholder `files.read_text(path)?` and `parse_player_data(text)?` must return compatible error types or have an explicitly defined error conversion. This example illustrates syntax, not an implicit promise that arbitrary error enums auto-convert.

## 59. Example: arrays, dictionaries, readonly parameters, and iteration

```python
const party_size = 4

func print_party(const names: [string, party_size]):
    for name, index in names:
        print("{index}: {name}")

func main():
    const party: [string, party_size] = [
        "Hero", "Mage", "Ranger", "Healer"
    ]

    print_party(party)

    var loot = ["sword", "shield", "potion"]
    loot.push("key")

    for item, index in reverse loot:
        print("{index}: {item}")

    var scores: {string: i32} = {
        "hero": 100,
        "mage": 75
    }

    scores["hero"] += 25

    for score, name in scores:
        print("{name}: {score:0000}")

    if scores.get("hero") is Ok(score):
        print("Hero has {score} points")
```

The readonly `names` parameter should not create unnecessary array copies. It has a fixed length determined by a compile-time constant.

## 60. Example: a callable, stateful object

```python
struct damage_multiplier:
    amount: i32 = 2
    private count: i32 = 0

func (m damage_multiplier) call(value: i32) -> i32:
    m.count++
    return multiply_value(value, m.amount)

private func multiply_value(value: i32, amount: i32) -> i32:
    return value * amount

func apply(value: i32, transform: func(i32) -> i32):
    print("Output: {transform(value)}")

func main():
    var double = damage_multiplier()
    apply(10, double)
    apply(25, double)
```

A callable struct can carry state and still satisfy a compatible callback signature. Whether method `call` and `func(i32) -> i32` use identical ABI representations is a compiler/runtime concern, not part of the application syntax.

## 61. Example: conditional values, patterns, and incomplete match semantics

```python
enum direction:
    up
    down
    left
    right

func direction_label(dir: direction) -> string:
    return match dir:
        direction.up: "Up"
        direction.down: "Down"
        direction.left: "Left"
        direction.right: "Right"

func update_direction(dir: direction):
    match dir:
        direction.up:
            print("Up")
        direction.down:
            print("Down")

    # Warning: left and right are not handled.
    # Unmatched values do nothing because this is a statement.
```

```python
func label_partial(dir: direction) -> string:
    return match dir:
        direction.up: "Up"
        direction.down: "Down"

    # Compiler warning for left/right.
    # Runtime VOID match error if left/right actually occurs.
```

No incomplete expression should silently synthesize `null`. Adding `_` supplies a default; covering all enum cases avoids warning.

## 62. Example: cleanup that survives early errors

```python
func load_map(path: string) -> result<string, error>:
    var file = files.open(path)?
    defer file.close()

    defer {
        print("Map load cleanup")
        log_flush()
    }

    var content = file.read_text()?
    return Ok(content)
```

On `Err`, `?` propagates the result while running all registered defers in reverse order. A panic handled by the runtime should also unwind language-managed cleanup where it is safe. A true native process crash cannot guarantee this.

## 63. Example: asynchronous scene loading with a task group

```python
use "tasks"

private async func load_world_data() -> result<string, error>:
    return await files.read_text("world.dat")

private async func load_audio_data() -> result<string, error>:
    return await files.read_text("audio.dat")

async func load_scene() -> result<bool, error>:
    with tasks.group() as jobs:
        var world_job = jobs.spawn(load_world_data)
        var audio_job = jobs.spawn(load_audio_data)

        var world = (await world_job)?
        var audio = (await audio_job)?

        print("World: {world}")
        print("Audio: {audio}")

    return Ok(true)
```

**Architecture note:** The precise async task-group API, how `jobs.spawn` consumes `async func` references, and async `with` cleanup grammar must be defined consistently before implementation. The example demonstrates the desired developer-facing simplicity, not a settled scheduling ABI.

## 64. Example: a small game-like style with very little code

```python
use "game"

const speed = 80

value struct vector2:
    x: f32
    y: f32

struct player:
    position: vector2
    health: i32 = 100

func (p player) move(dt: f32):
    if game.btn("left"):
        p.position.x -= speed * dt
    if game.btn("right"):
        p.position.x += speed * dt

var hero = player(position = vector2(x = 64, y = 64))

func update(dt: f32):
    hero.move(dt)

func draw():
    game.cls(0)
    game.spr("player", hero.position.x, hero.position.y)
    game.text("HP: {hero.health}", 4, 4)
```

The built-in game API is optional. This is intended to feel PICO-8-like without turning VOID 2.0 into a mandatory fantasy-console runtime.

---

# Part X. Decision register and outstanding questions

## 65. Explicitly accepted language decisions

| Area | Agreed decision |
|---|---|
| Philosophy | Easy and fun, small syntax, powerful functionality, compiler hides complexity |
| Naming | `snake_case` across all ordinary declarations |
| Block syntax | Python-like indentation; `defer { ... }` also available |
| Functions | `func`, receiver methods, no `this` |
| Scope | `var` declaration explicit, accidental shadowing is compile error |
| Constants | `const` readonly, usable to initialize/reassign mutable variables, manual restart after source edit |
| Numeric defaults | `i32` for common ints; `f32` for unsuffixed floating literals; `f` and `d` suffixes |
| Strings | One UTF-8 `string`, auto interpolation, C#-like formatting and triple-quoted text |
| Print | One `print()` with optional `end` |
| Objects | `struct` reference semantics, `value struct` value semantics, no classes needed |
| OOP | Methods, interfaces, single inheritance, composition, properties |
| Constructor/lifecycle | `init()`, `deinit()`, `with`, `defer`; GC memory separate from native cleanup |
| Visibility | Public by default, optional `public`, `private`, `internal`, `static` |
| Callable values | `func()` fields, anonymous funcs, closures, callable structs with `call()` |
| Events | `event ...: func(...)`, `+=`, `-=`, `.emit()` |
| Patterns | `is`, `match`, `Ok`/`Err`, typed captures, ranges and tagged enums |
| Match omissions | Statement warns/no-op; expression warns/runtime error if unmatched |
| Errors | `result<t,e>`, `Ok`, `Err`, propagation `?`, panic distinct from errors |
| Iteration | Only `for`: iterable/range traversal, Boolean condition, or unbounded `for:`; optional index/key, `reverse`, `break`, `continue` |
| Operators | `and`/`or`/`not`; arithmetic compound assignments; postfix statement-only `++`/`--` |
| Generics | `<>`, inference, interface constraints, no arbitrary parameter-count cap |
| Collections | `[]` growable/fixed arrays, `{}` dictionaries, no compiler-special stack/queue |
| Defaults | No uninitialized garbage data visible through safe VOID |
| Paths | Canonical `/` VFS imports, root and relative import styles, no namespace declarations |
| GC | Automatic, precise non-moving first implementation, low overhead and frame-time focus |
| Native FFI | C libraries through safe bindings, one public string, no ordinary unsafe pointers |
| Compilation | LLVM AOT release direction; ORC JIT development hot-reload direction |
| Hot reload | Function bodies auto-reload when compatible; no automatic restarts |
| Async | `async func`/`await` with implicit task mechanics, `spawn`, task groups, sync library |
| Tooling | First-class IntelliSense, docs, source-level compile/runtime diagnostics, stack traces |

## 66. Decisions that are *not* final and should not be assumed

This section matters especially when an AI assistant inherits this file without the preceding chat.

1. **`const` and deep immutability:** A readonly access path is agreed, but whether every `const` object is globally frozen under all aliases is not. Distinguish compile-time literals from runtime readonly managed objects and provide safe borrow rules.
2. **File/module import identity:** External `../` imports, symlinks, versioned packages, duplicate paths, case sensitivity on physical filesystems, and cross-project access need formal semantics.
3. **Exact grammar for module scopes:** Project-wide root constants are agreed; file-level mutable global initialization order, collisions, and cyclic dependencies are not complete.
4. **Static syntax:** The concept is accepted; `static func type.method()` grammar and visibility combinations need a parser contract.
5. **Overrides and polymorphism:** `extends` and interfaces are accepted. `override` and `super` are recommended; dispatch, override protection, and inheritance access rules need finalization.
6. **Construction:** `init()` is accepted. Named-field/custom-init interactions, fallible init return form, parent errors, and rollback cleanup need specification.
7. **`value struct` native ownership:** Ensure copying cannot double-release a native handle; initially restrict resource-owning value types or introduce an explicit safe transfer contract.
8. **Closures:** Mutable capture behavior is accepted; syntax/ABI of closure returns, lifetime, and hot-reload capture-layout compatibility need proof.
9. **Function call grammar:** Anonymous multiline functions passed directly into calls, named parameters, and default evaluation order need exact parse rules.
10. **Events:** Event-owner emission rule, subscriber ordering, mutation while emitting, weak versus strong callbacks, and unsubscribe identity must be resolved.
11. **Generic implementation:** Monomorphization/code-size strategy, interface constraints, recursive instantiation, and specialized versus shared representations remain open.
12. **Dictionary grammar:** Decide whether unquoted `name = value` is allowed alongside `"name": value`. Do not invent silent meaning for bare identifiers.
13. **Mixed/general collections:** `[]` and `{}` were discussed as general-purpose untyped options, but their dynamic type, boxing, and inference semantics are undecided.
14. **Array API spelling:** Both `add` and `push` appeared. The final name/alias set has not been locked. `push` and `pop` were explicitly liked.
15. **Iteration modification:** General mutation during traversal should fail; safe current-element deletion during reverse traversal was suggested but not formally accepted/tested.
16. **Pattern forms:** Range, struct-field, list/rest and dictionary patterns are agreed directions; precise grammar/binding/copy rules need specification.
17. **Numeric conversion policy:** Literal contextual typing is agreed. Detailed overflow, signed/unsigned, `NaN`, truncation, `as`/`as?` results and float equality/hash need tests.
18. **UTF-8 semantics:** One string is agreed; indexing unit, grapheme handling, normalization, formatting grammar, and C ABI encoding/ownership details remain open.
19. **Panic and defer:** Remaining defers and original errors should be preserved when safe; native unwinding/ABI/cleanup error aggregation need implementation design.
20. **Nullable/`Ok` overloading:** The `Ok` pattern is intentionally allowed for nullable values and results; representation and type-checker disambiguation need explicit formal rules.
21. **Async API:** `async func`, `await`, and `spawn` are agreed; exact scheduling start semantics, task group binding, async `with`, and cancellation observation points remain to specify.
22. **Hot-reload implementation:** Compatible-body-only behavior is agreed. Dispatch indirection, JIT memory retirement, cross-thread publication, and metadata lifetime need early proof.
23. **GC details:** Non-moving precise tracing is initial direction, not a permanent ban on other collectors. Root tracking, safepoint lowering, allocator layout, concurrency, and barriers need benchmarks and proof.
24. **FFI details:** No user-facing pointers/C strings is agreed; trusted native binding grammar, ownership metadata, foreign threads, and callback trampoline semantics are not finished.
25. **Tooling/packaging:** LSP, docs and extension discovery are goals; registry protocol, package manifests, trust verification, and compiler CLI spelling remain proposals.
26. **Module-as-scene adapter:** A module directly satisfying a scene interface and `func call()` at file level was explored, but the user corrected the discussion to favor **struct callback fields wired to private module functions**. Do not implement module-callable semantics as a locked feature without explicit reapproval.
27. **Empty function bodies:** The exact placeholder syntax is not settled; no need to invent a `pass` or `...` statement until parser design requires it.
28. **Operator overloading:** No general user-defined operator suite has been agreed. Avoid treating one as required just because structs and generics exist.
29. **Error diagnostics codes:** Code examples use illustrative `VOID...` IDs. The real diagnostic catalog is not yet allocated.
30. **Compiler implementation language and LLVM integration:** Not yet chosen. The C API and C++ API each have costs; evaluate concretely against hot reload and GC requirements.

## 67. Resolved changes and corrections from the discussion

To avoid future context drift, here are ideas explicitly changed or clarified:

- **VOIDC 1.0 versus VOID 2.0:** Do not conflate them. VOIDC 1.0 was intended to preserve C# semantics and GC while transpiling to C; VOID 2.0 is a separate, simpler native language design.
- **`const handler: func(i32)` as an event:** Proposed briefly, then replaced with explicit `event handler: func(i32)` to avoid context-dependent declaration meaning.
- **Incomplete `match` expression returning `null`:** Replaced with a compiler warning plus runtime failure if an unmatched value occurs.
- **Shadowing:** The designer explicitly wants `var health` redeclaration within a nested visible scope to be an error, while `health = 25` is valid.
- **Stack/queue as core types:** Removed from the minimal built-in collection set. `push`/`pop` and optional libraries suffice.
- **User-managed `cstring` and exposed `unsafe`:** Rejected for ordinary application code. Trusted bindings still need low-level machinery internally.
- **`struct` storage based on field initialization:** Explicitly corrected. Reference/value semantics matter; initializer presence does not determine stack/heap placement.
- **GC `deinit` behavior:** `deinit` is deterministic resource cleanup via explicit scope mechanisms, not a guaranteed GC finalizer.
- **File as a scene:** The user preferred retaining the straightforward `scene` struct with callback fields referencing private functions. A special compiler-provided file-scene adapter is not confirmed.
- **Default floating literal `0.5`:** Accepted as `f32` by default, but type context can request `f64`; don't reject everyday unsuffixed decimal literals.
- **One loop keyword:** Earlier drafts used `while` for Boolean-controlled repetition. The designer explicitly corrected this: all loops use `for` (`for condition:`, `for item in collection:`, or `for:`). `while` is not a language loop keyword.
- **Full property sugar or constructor overload forests:** Intentionally avoided pending a concrete need.

## 68. Compact vocabulary for continuing the project

**Reference struct:** `struct player` with managed identity and reference semantics.  
**Value struct:** `value struct vector2` with copy semantics and likely inline storage.  
**VFS module:** Compiler-managed path identity using `/` relative to the project root.  
**Readonly:** `const` binding/access view that cannot be mutated through that binding.  
**Function value:** A `func(...)` callable, anonymous closure, method reference, or compatible callable struct.  
**Event:** Zero-or-more subscriptions to a declared function signature; `.emit()` notifies subscribers.  
**Result:** `result<t,e>` carrying `Ok(value)` or `Err(error)`.  
**Nullable:** `t?` can contain `null`; a non-null value can match `Ok(binding)`.  
**GC:** Automatic managed-memory tracing, initially precise/non-moving/stop-the-world.  
**Native binding:** Trusted declaration + ABI bridge wrapping a C library and its ownership/encoding rules.  
**Hot reload:** Development-mode native function body replacement under a stable identity without application restart.  
**IR:** VOID's own small typed intermediate representation shared by LLVM AOT/JIT compilation.  
**LSP:** Language Server Protocol tooling reusing the compiler frontend.

---

# Part XI. Suggested rules for any future assistant or implementation session

1. **Read this entire file before proposing foundational language changes.** Do not infer decisions from a few isolated code snippets.
2. **Respect decisions labeled AGREED.** If a proposed change conflicts, explain the conflict, tradeoff, and required approval.
3. **Distinguish design from implementation.** This bible is not proof that a compiler/runtime already exists.
4. **Keep syntax lowercase snake case**, except explicitly accepted special `Ok`/`Err` patterns.
5. **Never reintroduce pointers, unsafe syntax, or C strings into normal VOID code** merely to achieve performance or library access.
6. **Keep the developer experience source-first.** No normal errors expressed in C or LLVM terms.
7. **Preserve hot-reload semantics:** no automatic restarts; incompatible edits leave the running state unchanged.
8. **Respect GC semantics:** optimizing allocations does not authorize removing memory safety or manualizing ordinary memory management.
9. **Do not confuse public-by-default with unrestricted mutable globals.** Scope, access, and root-constant visibility are separate rules.
10. **Be conservative about invented syntax.** Mark it RECOMMENDED/OPEN until explicitly accepted.
11. **Maintain cross-platform VFS `/` paths** and test resolver behavior on Windows and Unix-like filesystems.
12. **Use real evidence for performance and LLVM/runtime claims.** Do not promise Go parity or zero-overhead JIT simply because the backend is LLVM.
13. **Prefer focused, testable implementation milestones** with cumulative audits over major unchecked rewrites.
14. **Avoid em dashes** in examples, documentation, drafts and user-facing project text, in keeping with the author's writing preference.
15. **Ask or report only genuine missing project facts.** Never bluff about compiler behavior, GitHub state, or test results.

## 69. Single-page architecture reminder

```text
VOID 2.0

Developer goals:
  Easy to write + easy to read + easy to debug + fun + fast

Syntax:
  snake_case, func, var, const, struct, value struct,
  interface, property, if, match, is, for, reverse,
  result<>, Ok/Err, defer, with, init/deinit,
  [] growable/fixed, {} dictionary, string interpolation,
  event, async func, await, spawn

Imports:
  /project/root.void and ../relative.void
  Internal folder-based module identities, no namespaces

Memory:
  Automatic GC, precise non-moving initial collector
  Thread-local small allocation, inline value storage
  No ordinary pointers, manual free, unsafe, cstring

Native:
  Reusable trusted C bindings + VOID wrappers
  Opaque handles + deinit + native callback trampolines

Compiler:
  One frontend -> small typed VOID IR -> LLVM
    AOT release        ORC JIT development

Hot reload:
  Compatible function bodies automatically replace
  Const/layout/signature changes require manual restart
  No auto restart, no popup nagging, old code retained safely

Tooling:
  Shared frontend/LSP, ## docs, completion, source diagnostics,
  clickable stack traces, extension package discovery

Implementation strategy:
  Four early proofs (LLVM, GC, hot reload, C ABI)
  Then milestone-driven frontend, native code, runtime,
  managed types, live compile, advanced syntax, concurrency,
  ecosystem, optimization and cross-platform audits
```

---

## Closing design statement

**VOID 2.0 should be small on the surface and sophisticated underneath.** A beginner should be able to write a game, access a dictionary, call a native library, and understand a compiler error without knowing memory layouts, C headers, linkers, GC safepoints, or JIT machinery. An experienced developer should still have precise numeric types, generic abstractions, predictable value/reference semantics, native performance, asynchronous concurrency, flexible modules, and reliable IDE tooling.

The next practical activity is **not** to add more language keywords. It is to convert the agreed rules and open items in this bible into a precise language specification, establish a small compiler/frontend skeleton, and complete the four architectural proofs before committing to large-scale implementation.

*End of design bible snapshot.*

## Implementation status addendum: 10 October 2026

The design snapshot above is preserved. The initial compiler foundation now
provides executable LLVM 19.1.7 AOT/JIT architectural proofs, generation leases
for native function replacement, explicit precise root tracking under LLVM O2,
and a trusted C ABI fixture. These are experimental implementation mechanisms,
not completion of the designed language, GC or hot reload system. No `.void`
source syntax is compiled yet, and no language semantics were changed.

The compiler implementation uses C++20 and the LLVM C++ API. See
[DECISIONS.md](DECISIONS.md), [WORK_LOG.md](WORK_LOG.md), and
[KNOWN_ISSUES.md](KNOWN_ISSUES.md) for implementation scope and actual evidence.
