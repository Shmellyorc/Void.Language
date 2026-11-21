# VOID language specification

This document specifies implemented VOID source behavior through G93, with G88
library API changes documented in the [modernization report](projects/G88_STDLIB_PACKAGES_MODERNIZATION.md).
It is organized by language concept. Feature guides linked from each section
supply detailed rules and boundary cases; implementation reports record evidence
and platform qualification separately.

The [pre-G88 Bible](LANGUAGE_BIBLE_HISTORY.md) preserves the complete original
workshop discussion, approvals, superseded proposals and status addenda. The
[owner decisions](DECISIONS.md) and [current engineering instructions](WRENCH_CLANKER.md)
remain available. Planned syntax is confined to [deferred design](#deferred-design).

## Contents

- [Source syntax and names](#source-syntax-and-names)
- [Bindings, defaults and authority](#bindings-defaults-and-authority)
- [Numbers and operators](#numbers-and-operators)
- [Strings and display](#strings-and-display)
- [Modules and applications](#modules-and-applications)
- [Functions and calls](#functions-and-calls)
- [Structs, construction and members](#structs-construction-and-members)
- [Interfaces and generics](#interfaces-and-generics)
- [Enums and flags](#enums-and-flags)
- [Nullable values and results](#nullable-values-and-results)
- [Control flow and patterns](#control-flow-and-patterns)
- [Collections and iteration](#collections-and-iteration)
- [Resources and memory](#resources-and-memory)
- [Async and workers](#async-and-workers)
- [Conditional compilation](#conditional-compilation)
- [Native interoperability](#native-interoperability)
- [Reload and compiler architecture](#reload-and-compiler-architecture)
- [Source documentation and editors](#source-documentation-and-editors)
- [Deferred design](#deferred-design)

## Source syntax and names

VOID uses indentation and `:` to introduce bodies. Line breaks terminate ordinary
statements; balanced calls, literals and parenthesized expressions can span lines.
`#` introduces a comment and `##` public documentation. An ordinary body needs a
statement; `pass` explicitly supplies a no-op. Interface requirements are bodyless.
Keywords and types are case-sensitive. Source identifiers use lowercase ASCII
letters, digits and `_`, beginning with a letter or `_`; uppercase and Unicode
identifiers are rejected. Public API words use snake_case by convention.

```void
func main() -> i32:
    var score: i32 = 7
    if score > 0:
        print("score", score)
    return 0
```

Declarations cannot shadow a visible binding by redeclaring its name in a nested
scope. Assign to the existing binding or choose a different name. This also
matters for parameter names that conflict with visible type/constructor names.
Declarations, rather than filename conventions, establish most source identities.
See [modules](MODULE_SYSTEM.md) for lookup and import precedence.

## Bindings, defaults and authority

`var` declares writable storage. `const` declares a readonly binding and preserves
deep readonly authority through referenced children. `const` parameters borrow
readonly access. Copying a value struct or fixed array snapshots its own value
storage; referenced children retain their identities and readonly capabilities.
A writable independent scalar copy does not grant writable access to a readonly
referenced child. Managed references, lists and dictionaries retain shared identity
on assignment.

```void
value struct point:
    x: i32
    y: i32

func main():
    const origin = point(x = 2, y = 3)
    var moved = origin
    moved.x += 1
    print(origin.x, moved.x)
```

Types can be inferred from concrete initializers. Safe defaults include numeric
zero, false, empty strings/collections and null for nullable types. Required
nonnullable managed references and function values need initialization. Value
structs follow their field/default/custom initializer contract; defaulting is not
permission to read incomplete required children. Module `const` expressions use
the supported pure compile-time evaluator. Module `var` and static fields use
runtime initialization.

Outermost `var` in each reachable module declares application-owned runtime
storage. Startup initializes import components dependency-first, using canonical
module order within cycles and source order within each module. Globals initialize
before script statements or selected main. Reading an unready global fails; a
failed startup does not replay successful earlier initializers. Checking and
editor queries execute no startup effects. Aliases of a module share its storage.
Workers cannot access runtime globals, including through helpers or methods.

See [value semantics](VALUE_SEMANTICS.md), [reference-containing values](REFERENCE_VALUE_STRUCTS.md),
[managed readonly views](MANAGED_STRUCTS.md) and [runtime globals](RUNTIME_GLOBALS.md).

## Numbers and operators

The integer types are `i8`, `i16`, `i32`, `i64`, `u8`, `u16`, `u32`, `u64`.
Floating types are `f32` and `f64`; `bool` is separate. Ordinary unsuffixed numeric
literals default to `i32` or `f32` when context does not supply an exact type.
Contextual literals must fit their target. Numeric mixing and narrowing require
the documented conversions; Boolean values are not numeric truth values.

Integer arithmetic retains fixed-width behavior. Signed `MIN / -1` yields `MIN`,
and `MIN % -1` yields zero. Integer division or remainder by zero uses checked
failures. Floating arithmetic follows the implemented IEEE rules. Each numeric
type exposes `min_value`, `max_value`,
`abs` and `clamp`; signed minimum `abs` fails, and clamp validates its bounds
before propagating a NaN input. Floating limits are finite extrema.

Integer `&`, `|`, `^`, `~`, `<<` and `>>` preserve the exact operand width.
Binary bitwise operands have the same integer type. Shift counts are integers,
normalized modulo the left operand's width, including negative signed counts;
signed right shift is arithmetic and unsigned right shift logical. Bool, float
and enums are not integer shift operands; extract enum `.value` explicitly.
Compound assignments require writable storage. Postfix `++` and `--` are
statements, not value expressions; prefix increment is unsupported.

From weaker to stronger precedence: `or`, `and`, comparison/`is`, `|`, `^`, `&`,
shifts, addition/subtraction, multiplication/division/remainder, conversions.
`and` and `or` short-circuit. Parentheses make mixed masks and comparisons clear.

Custom operators use the [owner/operator contract](stage10/OPERATORS.md) and
[generic owner amendment](collections/GENERIC_OPERATORS.md).
They preserve exact owner lookup, direct-import authority, readonly rules and
value-place behavior. They do not introduce implicit conversion chains, overload
sets or custom dictionary comparers. See [numeric types](NUMERIC_TYPES.md) and
[ordinary calls and numeric prelude](STAGE8_CALLS_NUMERIC.md).

## Strings and display

`string` is immutable validated UTF-8. Unicode-scalar operations and byte lengths
are distinct. `to_utf8()` supplies owned bytes; `string.from_utf8(bytes)` returns a
checked result. Embedded NUL is valid language text; native APIs can impose
additional NUL-free constraints. There is no source `cstring` type.

Double-quoted strings support interpolation such as `"score {score}"`. Multiline
strings preserve their lexical extent, including source-looking text in their
contents. `print`, `format` and string conversion use supported compiler-owned
display hooks; arbitrary values are not automatically printable. Numeric/flags
enums expose `.name` for text and `.value` for their backing integer.

See [strings](STRINGS.md), [display and formatting](stage10/DISPLAY_FORMATTING.md)
and [text/codecs](text-encoding/API.md). Text parsing uses checked APIs such as
`convert.parse<i32>(text)`; casting a string does not parse it.

<a id="10-project-entry-point-and-vfs-imports-agreed"></a>
## Modules and applications

`use console`, `use net/tcp`, quoted project-relative imports and aliases such as
`use collections as c` all use the compiler-owned source graph. Standard modules,
project sources, source packages and native providers have separate identities
and authority. Importing package source alone grants no native execution permission.
Explicit aliases provide qualified lookup. `private` is file-private for ordinary
module declarations; `internal` follows the owning library domain. Public is the
default. Members retain their documented owner access rules.

A selected application's entry module provides root public/internal constants and
globals to project modules. Imported module declarations and root names retain
lookup precedence and ambiguity diagnostics. Imports do not execute entry scripts
as a substitute for the selected application.

Manifest projects use `void.project.toml`. Applications discover one valid
synchronous main, regardless of filename, or diagnose ambiguity. Libraries check
their source union without inventing an executable main; `package.entry` selects
their import surface. Manifestless directory selection uses `main.void`; explicit
source-file selection remains supported. Script-style entry and explicit main
cannot compete in the same selected program. `main() -> i32` returns on every
reachable path; no-result completion exits zero. Ordinary CLI run/build does not
implicitly host an async root. `--app` is removed and rejected.

Source package maintenance, lock identity, generated library entries and explicit
native requirements follow [local libraries](LOCAL_LIBRARIES.md),
[CLI](CLI.md), [module system](MODULE_SYSTEM.md) and
[application workflow](STAGE9_APPLICATION_WORKFLOW.md).

## Functions and calls

`func name(parameters) -> type:` declares a function; omit the result annotation
for no result. Receiver methods use `func (self owner) method(...)`. Functions and
methods share positional-prefix/named-argument binding. Named arguments may be
written in any order after the positional prefix; supplied expressions evaluate
once in written order before binding to parameter order. Unknown/duplicate labels,
positional arguments after named arguments, missing arguments and type mismatches
diagnose. Required parameters precede defaults. Defaults are checked pure
expressions in the declaring module. Concrete defaults may infer a nongeneric
parameter type; generic parameters remain explicit.

```void
func scale(value: i32, factor: i32 = 2) -> i32:
    return value * factor

func main():
    print(scale(factor = 3, value = 7))
```

Function types include their exact parameter/result and readonly authority.
Named functions, supported bound methods and anonymous `func` expressions are
first-class callable values. Closures preserve the documented shared capture
cells and lifetimes. Escaping closures made in a value receiver capture an
independent receiver snapshot, shared by copies of that closure. Managed receiver
captures retain object identity. Async callable values and callable transfers
across task boundaries remain restricted.

<a id="15-multiple-return-values-not-first-class-tuples-agreed"></a>
Multiple returns are transient return packs, not general first-class tuples.
They follow the exact declaration/binding rules in [advanced language](ADVANCED_LANGUAGE.md).
Use a value struct when a result needs a reusable stored record shape.
See [receiver methods](RECEIVER_METHODS.md), [callable/function behavior](ADVANCED_LANGUAGE.md)
and [calls](STAGE8_CALLS_NUMERIC.md).

### Pure compile-time functions (G93)

`compile func` marks an ordinary source function as eligible for required constant
contexts. It uses the same body, exact types, argument/default mapping, generics,
source ownership and arithmetic as an ordinary runtime call.

```void
compile func square(value: i32) -> i32:
    return value * value

const answer = square(8)

func main():
    var width = 9
    print(answer, square(width))
```

Public/private/internal modifiers precede `compile func`. Eligible pure value
receiver methods use `compile func (d dimensions) area() -> i32:`; ordinary and
generic value types retain their existing declarations. Generic functions and
methods are checked for their concrete operations at specialization. All concrete
marked bodies are purity checked, including unused bodies and unselected runtime
branches. Calls to unmarked functions remain excluded from required constants.

Supported constant calls compose with module constants, field and parameter
defaults, fixed-array extents, numeric/flags discriminants and existing pure value,
string, enum and collection templates. Locals and ordinary runtime calls retain
their runtime behavior. Local `const` still means readonly runtime initialization;
it does not request evaluation during compilation.

The supported body subset includes owned local values, local assignment, returns
and multiple returns, Boolean branches, conditional/range/fixed-array loops,
recursion, supported expressions and nested marked calls. Receiver mutation,
mutable reference storage, managed identities, custom initializers/operators,
closures, patterns, resource/defer scopes, async work, native calls, globals and
observable I/O are excluded. Compiler-owned collection templates do not allocate
Runtime objects during evaluation. No reference can escape a compiler temporary.

Evaluation has finite call, nesting, step, temporary-value and graph budgets.
Exhaustion produces a source diagnostic; editor cancellation stops constant work.
G87 selection runs before declarations are captured, so inactive marked functions
remain excluded. No `compile const`, `compile var`, `compile struct`, `comptime`,
reflection or macro language is introduced. See
[G93 semantics, limits and qualification](projects/G93_COMPILE_FUNCTIONS.md) and
[the runnable example](../examples/compile_functions/main.void).

## Structs, construction and members

An ordinary `struct` has managed reference identity. A `value struct` has copied
inline value storage. Fields do not choose reference/value semantics based on
whether they have initializers. Construction uses `owner(...)` without `new`.
Without a custom initializer, named fields supply explicit values and omitted
fields follow safe defaults/readiness rules. Positional field construction is not
an alternative overload mechanism.

A custom `func (self owner) init(...)` owns constructor parameters. It runs once
after prescribed field preparation; ordinary copies never rerun it. Required
children and managed construction views obey readiness and escape restrictions.
A fallible initializer returns `result<bool,e>`; construction produces
`result<owner,e>`. Success is the `Ok` case (`Ok(true)` by convention, with `Ok(false)`
also successful); typed failure is `Err(error)`. Failed construction does not expose a partial
owner or run its destructor as though fully constructed. Prepared children and
native acquisitions retain their specified cleanup rules.

Properties use an external `property (self owner) name: type:` declaration with
`get:` and/or `set:`. Receiver mutation requires writable value storage or the
permitted managed authority. Getter-only properties do not create writable places.
Ordinary fields/methods, properties and operators follow one member lookup path.

`using` embeds a named or unnamed ordinary stored component and promotes members
one hop. Direct members take precedence; competing promoted names diagnose at
access before argument/result types can select a component. Composition adds no
subtyping, implicit conversions or transitive forwarding. Managed inheritance uses
`extends`, the documented `super.init` prelude and virtual/override rules; it
preserves complete-object identity and base-view authority.

Static fields are declared inside a struct with `static name` or `static const`.
External static methods use `static func owner.name(...)`. Type access selects
static members, while instance access selects instance members. Static fields
are excluded from layouts and field construction. Runtime static initialization,
readonly authority, generic-owner specialization and restart dependencies follow
[static members](STATIC_MEMBERS.md).

Events are explicit `event name: func(...)` declarations. Registration/removal
use the supported callable identities; `emit` visits the ordered event snapshot
and preserves mutation/callback lifetime rules. An ordinary function-valued field
is not implicitly an event or a callable module.

See [value structs](VALUE_STRUCTS.md), [managed structs](MANAGED_STRUCTS.md),
[initialization](INITIALIZATION.md), [managed initialization](MANAGED_INITIALIZATION.md),
[composition](COMPOSITION.md), [inheritance](INHERITANCE.md),
[properties](PROPERTIES.md) and [advanced language](ADVANCED_LANGUAGE.md).

<a id="13-interfaces-agreed-phase-21-bounded-implementation"></a>
## Interfaces and generics

Interfaces are nominal declarations with nonempty bodyless synchronous method
requirements. Structural conformance selects accessible exact signatures; it does
not require `implements`. Parameter spelling is irrelevant, while type, order,
nullability, readonly authority and result type must match. No bodies, overloads,
requirement defaults or lifecycle requirements are introduced. Managed interface
views retain complete-object identity and witness bindings. Ordinary standalone
value boxing/erasure and arbitrary interface downcasts remain unsupported.

Functions, managed/value structs, enums and interfaces support lowercase generic
parameters. Inference uses concrete arguments and nested type patterns; ambiguous
empty/null/return-only uses need explicit arguments. Exact specializations preserve
nominal identities and existing authority. Generic bodies are checked when
specialized, with declaration and requesting locations in diagnostics; declaration
syntax and headers are checked independently. Generic operators use their exact
substituted public owner under the documented authorization contract.

```void
interface reader<t>:
    func read() -> t

struct cell:
    value: i32

func (self cell) read() -> i32:
    return self.value

func read_one<t>(const input: reader<t>) -> t:
    return input.read()

func main():
    var source = cell(value = 7)
    const view: reader<i32> = source
    print(read_one<i32>(view))
```

Constraints such as `<t: readable>` use the existing structural member rules.
`has Component` tests nominal embedded composition; `has Interface` tests
structural conformance. These use type information, grant no additional authority
and do not refine an ordinary runtime branch or skip checking one branch.
See [generics](GENERICS.md), [interfaces](INTERFACES.md) and
[has/ergonomics](STAGE8_ERGONOMICS.md).

## Enums and flags

Tagged enums have nominal cases with optional typed payloads. Generic tagged
enums infer from payloads or accept explicit type arguments. Numeric enums have
one of eight fixed-width integer backings and unique explicit decimal
discriminants. Their identity remains distinct from the backing integer and from
other enum declarations/specializations.

```void
enum reading<t>:
    missing
    present(data: t)

enum status(i32):
    ready = 1
    failed = 7

flags enum access(u8):
    none = 0
    read
    write
    read_write = read | write

func main():
    var chosen = reading.present(7)
    if chosen is reading.present(var amount):
        print(amount)
    var mask = access.read | access.write
    print(mask.has(access.read_write), status.ready.value)
```

`raw as? status` yields `Some(member)` only for a representable declared value,
otherwise `None`. `as` checks the same conditions and fails if invalid. Enum to
integer conversion is explicit and checked; `.value` extracts the exact backing
integer. Direct cross-enum casts are unsupported.

`flags enum` defaults to `u32`. Omitted flag values select the next unused power
of two above previously used bits. Explicit members may combine earlier members.
Same-type `&`, `|`, `^`, `~` and compounds preserve the flags type. All backing-width
bit patterns, including unnamed combinations and unknown bits, are valid flags.
Conversions enforce numeric bounds rather than filtering declared bits. `has`
tests all requested bits, `has_any` any requested bit; zero behaves accordingly.
`with`, `without` and `toggled` return new values. A flags match needs a catchall.

`.name` gives the declared member/case name, or decimal backing text for unnamed
flags. `is_defined()` tests exact declared membership. `Enum.values()` lists
numeric or payload-free cases in declaration order without inventing payloads.
Tagged equality compares active cases/payloads in order with ordinary supported
equality; custom enum operators take precedence. Unsupported payload equality
diagnoses. Eligible enum dictionary keys use the existing data-only key rules.
See [enums and flags](ENUMS.md).

## Nullable values and results

`T?` contains a supported `T` or `null`. Scalars, strings, structs, enums, arrays,
lists, dictionaries, interfaces and function values retain their respective
representation and authority rules. A value of `T` lifts to that same `T?`; null
requires nullable context. Null differs from zero, false, empty text and empty
collections. Nullable recursive inline containment must remain finite.

`if input is Ok(var present):` binds a nullable payload. `input as T` checks
presence before extraction; `input as? T` returns `Some(payload)` or `None`.
Presence checks do not introduce implicit dereference or general flow narrowing.
Canonical `option<t>` uses `Some` and `None`; `result<t,e>` uses `Ok` and `Err`.
Nullable presence, option absence and result failure remain separate contracts.

```void
func positive(value: i32) -> result<i32, string>:
    if value < 1:
        return Err("expected a positive number")
    return Ok(value)

func doubled(value: i32) -> result<i32, string>:
    var checked = positive(value)?
    return Ok(checked * 2)

func main():
    var maybe: i32? = 0
    if maybe is Ok(var present):
        print(present)
    if doubled(7) is Ok(var answer):
        print(answer)
```

`?` propagates the supported checked failure through a compatible enclosing
result boundary and runs required scope cleanup. It does not turn a runtime
failure or cancellation into an arbitrary typed error. See
[nullable types](NULLABLE_TYPES.md), [fallible construction](INITIALIZATION.md#typed-fallible-initialization-g84)
and [advanced language](ADVANCED_LANGUAGE.md).

## Control flow and patterns

`if`, `else if` and `else` use Boolean conditions. Conditional expressions select
one reached value under the supported typing rules. There is one loop keyword:
`for`. It supports Boolean repetition, unbounded repetition and iteration over a
source. `break`, `continue`, `return` and `pass` have ordinary lexical meaning;
cleanup still runs on exits.

`is` tests patterns and binds names only on matching paths. `match` supports
case/payload, type/guard, literal, collection and other implemented patterns.
Pattern bindings obey no-shadowing and authority rules. A statement match can
fall through if nothing matches. An incomplete expression match warns and
fails at runtime if no arm matches; it never fabricates null or a default result.
Irrefutable return analysis and exact pattern support follow
[advanced language](ADVANCED_LANGUAGE.md).

```void
func main():
    var total = 0
    for value in 0..<4:
        total += value
    for total < 8:
        total += 1
    print(total)
```

Ranges use `..<` for exclusive and `..` for inclusive endpoints, preserving the
implemented exact numeric/range rules. `for item, index in source:` binds the
item first, then index/key. `reverse` uses an actual reverse capability; it does
not silently copy or buffer an arbitrary source.

## Collections and iteration

`[T]` is a growable list; `[T, N]` is a fixed array. Lists share outer identity on
assignment; fixed arrays snapshot value storage. Lists support checked indexing,
push/pop, insert/remove, clear, safe access, slices and other
[documented operations](ARRAY_OPERATIONS.md). Fixed arrays cannot resize and do
not silently coerce to a growable list. An explicit slice makes a new outer list.
Indices count elements, with supported negative positions normalized relative to
the end. Slices preserve inclusive/exclusive and reverse bounds under the exact
[array contract](FIXED_ARRAYS.md).

`{K: V}` declares a dictionary. Literals evaluate left to right, key before value;
later duplicate keys replace values without moving the first insertion position.
Dictionary assignment shares identity. Enumeration preserves insertion order;
`for value, key in dictionary:` binds value first. Removal/reinsertion and live
structural mutation follow the checked iteration rules. Const observations do not
grant writable child references.

Keys include exact integers, bool, strings, managed identity and eligible value
structs/enums with recursively supported fields. Float, collection, interface,
custom-equality owners and canonical option/result keys retain their exclusions.
Supported nullable value keys preserve presence. Hashing follows compiler-owned
data schemas and offers no public custom comparer or hash callback.

Supported matching fixed-array/list/dictionary types implement content equality.
Sequences compare ordered elements, dictionaries compare logical key/value contents
independent of insertion order. Comparisons preserve ordinary element/custom
operators, short-circuiting, readonly authority and checked structural invalidation.
They grant no new dictionary-key eligibility. Recursive unsupported structural
shapes diagnose. See [dictionaries](DICTIONARIES.md) and
[collection equality/hashing](COLLECTION_EQUALITY.md).

Custom synchronous iteration acquires concrete state through `iterator()` or
`reverse_iterator()`. `next()` returns `option<T>`; optional `index()` provides
an explicit index/key. Protocol selection and structural constraints use existing
member lookup. Managed iterator `deinit` cleanup is scoped; no hidden buffering
is used. Async iteration is a separate protocol described below. See
[custom iteration](CUSTOM_ITERATION.md).

## Resources and memory

Managed allocations use the coordinated precise GC. Reachable managed children,
closures, interface witnesses and async frames retain their prescribed roots.
Value storage can contain managed references without turning them into untracked
native pointers. Assignment/rebinding alone does not invoke source cleanup.
GC reachability does not guarantee deterministic source `deinit()`.

Use `with` and `defer` for deterministic scope cleanup. Deferred calls capture
receiver/arguments when registered; deferred blocks read their shared captures
when executed. Cleanup order, primary/secondary failure handling and async drain
follow the existing lifecycle contract. Native owners retain explicit close/unload
semantics, alias closure and Runtime shutdown ownership. A copied wrapper is not
an independent native resource. Callbacks require unregister-and-drain barriers.

See [GC Runtime](GC_RUNTIME.md), [advanced language lifecycle](ADVANCED_LANGUAGE.md),
[native libraries](NATIVE_LIBRARIES.md) and the resource API's own source docs.

## Async and workers

`async func`, `await`, `spawn` and lexical `with tasks.group()` use one structured
Runtime scheduler. Ordinary async work runs on the owner with cooperative
suspension; eligible worker snapshots use the approved transfer/effect rules.
There are no detached source tasks or implicit async application startup.
The explicit embedding/root adapter hosts async examples and tests.

Start independent jobs before awaiting their results when concurrency is wanted.
Group exit joins admitted descendants. Typed `Err` is a successful task result,
not automatic sibling cancellation; runtime failure and cancellation have
separate paths. Cancellation drains entered native work and descendants before
resource destruction. A completion signal alone does not authorize retiring an
active execution frame. Retained task handles preserve repeated awaits/results.

`for await item in source:` acquires state through synchronous `async_iterator()`
or `reverse_async_iterator()`, then awaits its concrete managed `async next() ->
option<T>`. Each advancement finishes before optional synchronous `index()` and
the body. Cancellation drains advancement before synchronous state destruction.
There is no async destructor or generator syntax. Synchronous iteration never
implicitly awaits.

Generic async functions and concrete managed generic receivers specialize through
the ordinary pipeline. Async interface dispatch, async callable values, async
value receivers and callable task transfers remain rejected. `task<T>` in editor
labels describes an inferred opaque handle; it is not public annotation or
constructor syntax. See [async](ASYNC.md) and
[async iteration](CUSTOM_ITERATION.md#async-iteration--g86).

## Conditional compilation

`compile if`, `compile else if` and `compile else` select source for one immutable
configuration before parsing declarations, imports and generic bodies. Predicates
use `true`, `false`, `not`, `and`, `or`, parentheses, `target_os`, `target_arch`
and owning-manifest Boolean `feature.name` conditions, under the
[exact G87 grammar](projects/G87_CONDITIONAL_COMPILATION.md).

```void
compile if target_os == "linux":
    const host_name = "linux"
compile else if target_os == "windows":
    const host_name = "windows"
compile else:
    const host_name = "other"

func main():
    print(host_name)
```

Targets derive from the active native backend configuration, not arbitrary source
strings. Owning packages use their own manifest feature map; consumer features
are not injected into standard modules or dependencies. Unknown conditions and
invalid directive framing diagnose even where ordinary excluded text is opaque.
Excluded declarations/imports do not enter lookup, semantic tokens, dependencies
or initialization. Selected generic templates share that configuration for every
specialization; predicates cannot inspect generic types or ordinary values.

Configuration changes require recompilation and Runtime restart. Original source
spans and UTF-16 editor positions are preserved. Inactive regions receive compiler
metadata; formatting a conditional file currently returns no edits. G93
`compile func` declarations compose with this selection without changing predicates.

<a id="41-native-c-interoperability-agreed"></a>
## Native interoperability

Trusted native modules use admitted descriptors and the typed C bridge, preserving
C symbol identity, calling convention, primitive width/signedness, record layout,
resource ownership and callback lifetime rules. Host-admitted `native` declarations
are not general source permission to call an arbitrary library. Raw native addresses
remain behind audited providers and sealed owners.

A VOID enum is not assumed ABI-equivalent to an arbitrary C enum. Public wrappers
may accept typed enums/flags and explicitly extract the exact primitive at native
calls or C record construction. Raw C records retain their native primitive
fields. Externally specified identifiers and numeric sentinels remain exact;
converting a provider's open numeric result to a closed enum needs an explicit
policy for unknown values.

The verified raylib package uses the pinned header, parity assertions, ownership
adapters and source docs. Native platform/provider qualification remains specific
to the recorded build and host. Linux execution does not qualify Windows or macOS.
See [native libraries](NATIVE_LIBRARIES.md), [header import](CLI.md),
[raylib](../verified-packages/raylib/README.md) and its
[ownership contract](../verified-packages/raylib/OWNERSHIP.md).

## Reload and compiler architecture

The implementation has one compiler-owned frontend/source graph, typed IR,
validated LLVM lowering for ORC JIT and linked AOT, and coordinated Runtime.
LLVM/Clang/LLD 23.1.4 is the active pinned toolchain. Optimization cannot redefine
source semantics, ownership, roots or ABI. Tooling consumes compiler analysis;
it does not maintain a competing parser, resolver or type system.

Compatible body reload preserves declared state contracts and generation leases.
Changed layouts, initialization dependencies, project configuration and other
incompatible contracts require restart under the documented admission rules.
Running native callbacks and suspended work retain their required code lifetimes.
Automatic watch/restart behavior is not implied by the existence of reload APIs.
See [compiler/build architecture](SOURCE_COMPILER.md),
[analysis compatibility](ANALYSIS_COMPATIBILITY.md) and [Runtime](GC_RUNTIME.md).

## Source documentation and editors

`##` documentation belongs immediately before the public declaration it describes.
Use ordered `## @param name Description.` entries and `## @return Description.`
where applicable. Document actual input constraints, ownership, resource cleanup,
checked failure and native limitations. Declarations without a result need no
invented result tag. Member/case documentation uses the same source ownership.

Completion, hover, signature help, definition/reference navigation, semantic
highlighting, diagnostics and recovery are compiler-owned. Readable generic,
enum and inferred task labels preserve actual symbol identity. Editor versions
and extension presentation are qualified separately from protocol behavior.
See [language server](LANGUAGE_SERVER.md), [editor integration](EDITOR_INTEGRATION.md),
[diagnostics](DIAGNOSTICS.md) and [public library modules](README.md#standard-library-modules).

## Deferred design

The following are design directions or exclusions, not implemented source features.
The complete original rationale remains in [the historical Bible](LANGUAGE_BIBLE_HISTORY.md),
[decisions](DECISIONS.md) and the relevant proposals.

| Direction | Current boundary |
| --- | --- |
| Optional `if condition: return` | Deferred; use an indented body. |
| VOID Sections (`# section Name`, `# endsection`) | Approved organizational-comment direction; specialized tooling remains deferred. Comments do not create scope. |
| Stateless/method-only reuse and transitive promotion | Future design; `using` currently embeds storage and promotes one hop. |
| Standalone value-interface boxing, broad interface conversions | No general erasure/downcast protocol beyond current managed views and concrete constraints. |
| Weak references and broader collector tuning | Design direction; do not infer public weak-reference syntax or certified memory/latency bounds. |
| Detached tasks, generator syntax, async destructors | No such source facility; use structured groups and explicit iterator/resource protocols. |
| Module-as-scene/callable-module adapter | Unapproved idea; use structs with callback fields. |
| Public package registry/discovery and production distribution trust | Existing explicit source/native package workflow does not imply these services. |
| Automatic SIMD expansion, alternate compiler backends | Separate engineering/design work; neither changes the current language contract. |

<a id="historical-entry-point-discussion-12-october-2026-superseded-where-noted"></a>
The earlier entry-point debate is preserved at its
[historical section](LANGUAGE_BIBLE_HISTORY.md#historical-entry-point-discussion-12-october-2026-superseded-where-noted).
Use [modules and applications](#modules-and-applications) for the implemented rule.
There is no separate G91 milestone: its library/documentation scope is included
in [G88](projects/G88_STDLIB_PACKAGES_MODERNIZATION.md).
