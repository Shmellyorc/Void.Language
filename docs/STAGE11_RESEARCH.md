# Stage 11 research: runtime globals and the first library expansion

25 October 2026. **Research and recommendations only; no Stage 11 implementation
or new language semantics are approved by this document.** For independent Golden
Wrench review and Boss decisions. [Implementation proposal](STAGE11_ROADMAP.md).

## Baseline and authority

Stage 10 is officially locked at tested source
`d507240484f94aa4782bdb8828ea1ba0017bcda6`; lock record/current HEAD:
`909c0c33d882f4f3f2b3f25f656271d42968c0bf`. Boss's accepted result is seven jobs,
1,383/1,383 main CTest executions. The intervening lock commit changes three
documentation files only; inspected compiler/Runtime/library sources therefore
match the tested source tree. [Stage 10 acceptance](stage10/ACCEPTANCE.md).

Worktree note: a separate `stdlib/console.void` change appeared during the
investigation. At first inspection it added blank separator lines only, and its
nonblank lines matched the locked source exactly. It remained under concurrent
edit at closeout and was left untouched. This research authored only the two
Stage 11 documents and does not attribute that formatting edit to itself.

The accepted exclusions remain exclusions: native Windows/macOS Vim/Neovim host
execution and physical second-volume Windows verification were not certified.
Nothing in this research reruns or enlarges that acceptance.

Read the current [index](README.md), [Wrench governance](WRENCH_CLANKER.md),
[Research role](RESEARCH_CLANKER.md), [Bible](LANGUAGE_BIBLE.md),
[stdlib/global handoff](VOID_STDLIB_AND_GLOBAL_VARS_WRENCH_HANDOFF_2026-10-24.md)
and [independent review](VOID_WRENCH_REVIEW_CI4_STDLIB_GLOBALS_2026-10-24.md).
Earlier “Stage 10 incomplete” statements are historical. Current Boss authorization
covers this research; older “do not begin Stage 11” text does not prohibit it.

Two important corrections are already recorded in the handoff's current section:
nullable `is Ok(var value)` is supported, and `main(raw: [string])` is an ordinary
function declaration but **not a selected application entry**. Do not revive the
older review's nullable-pattern objection or assume argument-bearing main works.

## Verified current behavior

| Area | Actual source finding | Stage 11 consequence |
|---|---|---|
| Outermost `var` | Parser appends it to `$application`; ordinary functions cannot see that body's locals | A real global declaration, shared storage and lookup are missing |
| Imported `var` | `validate_graph` rejects imported executable top-level statements | Permit declarations without permitting arbitrary imported statements |
| Application selection | `$application` first; legacy `entry` otherwise suppresses main; selected main is synchronous, nongeneric, zero-argument, no-result/i32 | Removing globals from the script body changes entry classification and needs explicit migration rules |
| `const` | Pure module constants; runtime local readonly bindings and deep reference views | Neither becomes a mutable global or runtime initializer |
| IR/analysis | Module has functions/types/strings but no runtime-global declaration/storage list; analysis kinds have module/local constants and locals, no global-variable kind | Extend the existing typed model and owned metadata |
| GC | Runtime-owned Handle roots, Frame/Slot maps, complete object publication and reference schemas already exist | Globals can reuse traced storage and persistent handles |
| Runtime lifetime | JIT currently reuses `Impl::source_runtime`; replacement explicitly keeps `current.runtime_owner()` | Runtime identity alone cannot distinguish independently compiled programs |
| Native resources | Registry cleanup is explicit; Runtime shutdown drains/closes providers; Runtime destructor requires handles/participants gone | A global root is not a native destructor; release roots in a deliberate shutdown phase |
| Workers | Frontend propagates unsafe call graphs; native/indirect/task effects are already restricted | Add global-access effects to the same restrictions and independent IR proof |
| Async | Current worker queue resumes source task frames; no general native-I/O completion facility | `_async` cannot be implemented by calling blocking I/O on the owner |
| Imports | `use io/files` selects `/@stdlib/io/files.void`; `use lib/a` selects `/libs/a/a.void` | Keep the two source domains and their existing distinct spelling rules |
| Library | Actual official modules are `console.void` and `time.void` | New modules must add functionality rather than reproduce them |
| Tooling | Owned declarations/references, editor assistance, overlays, UTF-16 spans and guarded rename exist | Add global records and real module docs to the same compiler-owned service |

Source anchors: [parser/selection/graph](../src/frontend/compiler.cpp),
[IR module](../include/void/ir.hpp), [analysis API](../include/void/frontend.hpp),
[capability worklist](../src/frontend/capabilities.inc),
[IR capabilities](../src/ir/reference_capabilities.inc),
[Runtime roots](../include/void/runtime.hpp),
[coordination/shutdown](../src/runtime/coordination.cpp),
[native lifecycle](../src/runtime/native_binding.cpp),
[JIT compile/replacement](../src/backend/llvm/backend.cpp),
[invocation](../src/runtime/reload.cpp), [async](../src/runtime/async.cpp),
[source roots](../src/frontend/source.cpp), [tooling](../src/frontend/tooling.cpp).

### Bounded behavior probes performed

Eight expected-outcome checks reused the existing local Linux executable
`build/step42-ci96-debug/voidc`, SHA-256
`068b8f1578060dc3dfda1957d83a51b0d1fbf6f7afa1f1b22f4166922179046e`.
Fixtures existed only in a removed temporary directory. No build or cache-enabled
invocation occurred. This older local binary is **not asserted to be the locked
native binary**; current source above independently corroborates these behaviors.

| Probe | Observed result |
|---|---|
| `var score = 7` followed by a function returning `score` | check fails, VOID2002 unknown identifier |
| Import a module containing `var score = 7` | check fails, VOID4001 |
| Entry `var score = 7` plus zero-argument main | check fails, VOID4001 competing entry styles |
| Entry script declares/updates/prints its own `var` | check succeeds |
| Module `const score = 7`, main returns score | check succeeds |
| Ordinary `func main(raw: [string])` | check succeeds as a library declaration |
| Run that argument-bearing main | fails before execution, VOID4001 no entry |
| Declare a local in one if branch, assign it in its sibling | check fails, VOID2002 |

These are investigation controls, not new production acceptance tests. Existing
relevant test families include frontend/value semantics, modules, managed/reference
values, async/native lifecycle, Stage 8/9 application workflows, analysis/reload,
LSP sessions and Stage 10 console/formatting. Preserve their assertions; do not
turn old rejections into success without corresponding new semantic coverage.

## Recommended runtime-global semantics

### Declaration, visibility and copies

Use ordinary outermost `var`, in **every** source module, including standard and
project-local library modules. Add no `global` or `nonlocal` keyword. A variable
inside a function, if/for body or other block remains lexical. Struct fields retain
their own rules. Preserve the current no-shadowing rule and separate field namespace.

Use canonical module-plus-symbol identity. Aliases, repeated imports and diamonds
refer to the same binding. Own-file access is unqualified; imported access uses
the existing alias/qualified visibility rules. Recommend **no implicit project-wide
visibility for entry-file runtime vars**. Root constants keep their existing
special lookup. This prevents hidden mutable dependencies and makes `state.score`
explicit; Boss must choose rather than inherit the constant exception silently.
Public remains the default; top-level private is file-private; internal retains
the current project/standard-library boundary. No import grants native authority.

Infer/check types through the current expression checker. Permit omitted initializers
only where the existing local/default rules provide a complete safe value. Do not
invent null defaults for required nonnullable references. Type-inference cycles
need a clear diagnostic or explicit annotation, not a new global type system.
Module constants, field defaults and ordinary default parameters still cannot read
runtime globals as compile-time expressions.

Reads of value structs/fixed arrays produce ordinary independent value snapshots;
managed/string/list/dictionary/interface references retain existing identity and
authority. Stores preserve invariant readonly-leaf profiles and complete publication.
`const local = global_value` does not freeze the global or other mutable aliases.
Reading/rebinding through `var` cannot thaw an already-readonly reference leaf.

### What “once” means and where storage lives

Recommend one admitted program-global state per application Runtime, initialized
on first execution, not on parsing, checking, lowering, cache population or import
completion. Repeated calls into that running program retain state. A new CLI process
or fresh program instance gets fresh values. Compatible replacement retains the
same state and never replays initialization.

**Important host detail:** today's JIT shares its default source Runtime across
separate compilations. Do not use a process-static map, physical path, or matching
VFS names as authorization to merge their globals. Recommended small adjustment:
fresh initial compilation of a global-bearing program receives its own source
Runtime; replacement explicitly reuses the current Runtime as it already does.
Explicit host-supplied Runtime use must reject incompatible/new independent global
program admission unless the trusted host deliberately selects that same program
instance. Global-free compilation keeps existing behavior. If future hosts need
multiple unrelated global programs inside one Runtime, design that separately;
do not preemptively build a project-state service.

A feasible representation is a Runtime-owned table of existing Handle roots, one
complete hidden typed managed cell per global. Each cell contains its source value;
reuse descriptors, physical fields, heap stores and snapshot copies. Register null
root slots before evaluating initializers, then publish each complete cell after
its initializer succeeds. Unready cells have no source-readable payload. This
also handles reference-bearing aggregates without keeping a thread-local Frame
alive forever. Scalars can use the same simple representation initially; do not
add a native/managed hybrid for unmeasured savings.

The exact representation remains Keyboard's implementation choice. Required
invariants are precise roots, stable shared storage, no pointer into retired code,
and no serialized live Runtime state. A generation-owned LLVM global alone does
not establish those invariants. Runtime state must not retain NativeCode in a
cycle; layout/trace records are data, not JIT callbacks.

### Initialization order and failures: recommendation, not approval

Prefer eager deterministic initialization of all **reachable** globals before
the selected entry body/legacy entry/main. No filesystem-wide module discovery.
Initializers may perform ordinary synchronous expressions/calls, including print,
clock reads and recoverable I/O. They may read/write already-ready globals. They
may not enqueue tasks, await/spawn, or register reentrant native callbacks while
the graph is partially initialized. Explicit `boot()` after startup is the better
place for asynchronous/resource-heavy game setup.

Recommend this precise order:

1. Admit Runtime schemas/native permissions and establish global roots first.
2. Compute dependency-first groups from the **existing direct-import graph**.
3. Keep import strongly connected components legal; within a component order
   modules by canonical VFS identity. Break other ready-group ties by the same key.
4. Initialize each module's globals in source declaration order.
5. Start selected entry execution only after every reachable global is ready.

Do not silently reorder effectful initializers by a newly inferred global-read
graph. A reference to an unready global is an error, even when its type has a safe
default. Diagnose statically certain early reads where straightforward; enforce
the rest with small per-program/per-cell uninitialized/initializing/ready/failed
state checks. Include the initializer and target declaration in diagnostics.
Function-only import cycles and cycles containing independent globals remain legal.
An initialization read cycle fails instead of exposing a zero/null placeholder,
waiting forever, or recursively running another initializer lazily.

Go offers a useful comparison: dependency-aware variable initialization is possible,
but indirect dependencies require explicit language policy. VOID's simpler proposed
lexical rule deliberately avoids copying that entire analysis algorithm.
[Go initialization](https://go.dev/ref/spec#Package_initialization).

An initializer's normal `Err` result is ordinary data if its variable has result
type; it is not an automatic startup failure. There is no ambient result return
type for a file: reject bare initializer `?` rather than invent implicit unwrap
or change main's signature. A helper may handle/propagate errors inside its own
ordinary result-returning function. Allocation/checked Runtime failure aborts
startup, prevents entry execution and leaves the program failed. A later call
reports that owned failure; no automatic retry/replay of side effects. Restart
uses a fresh program instance. Earlier print/file effects are not rolled back.

This needs a real state guard even with mostly static checks: user helper calls,
virtual/interface dispatch and source callbacks can hide early accesses. Reentrant
entry during startup must fail clearly rather than deadlock behind its own once
lock. Only one owner thread initializes; no mutex is held while running source or
waiting in native I/O.

### Entry-script migration must be explicit

Recommend hoisting **all outermost var initializers** into the above startup phase;
retain the relative order of remaining entry statements. Thus:

```text
print("before")
var item = make_item()
print("after")
```

would run `make_item()` before both prints. That is an intentional semantic change,
not backward-compatible interleaving. Warn in migration notes/examples and obtain
Boss approval. A user needing the old timing declares a safely defaulted global
and assigns it at the desired entry statement, or moves the binding into a function.
Nested block vars remain local and are not hoisted.

After extraction, globals may coexist with main or legacy entry. Preserve current
precedence for actual executable statements and competing main diagnostics. With
no script statements, legacy entry retains precedence over main. Recommend a
selected entry containing only globals, with no explicit entry/main, initializes
them and exits zero; an imported global-only module never selects application
entry. Libraries remain checkable without entry. Typed i32 main still requires
explicit returns on all reachable paths; `[string]`/async main is not added.

### Workers, async and host threads

Runtime globals are not automatically atomic. Recommend owner-thread access for
global-bearing applications. Ordinary async activations on that owner can share
globals, with normal interleaving; `+=` is not an atomic operation. Preserve current
prepared-place/await restrictions and rooted capture semantics.

Reject **reads and writes** to mutable runtime globals from worker-reachable code,
including transitive helper/property/operator/hook accesses. Evaluating a global
scalar on the owner and passing its value snapshot into an otherwise legal worker
remains permitted. Unused globals in an imported module do not make all its pure
functions worker-unsafe. Module constants remain usable.

Use the existing finite effect/call graph, adding explicit global origins and
read/write/escape effects; independent IR must derive/enforce them too. Foreign
native callbacks or simultaneous host calls that reach global state need checked
owner admission, not unsynchronized C++ memory access. Same-owner completed-state
reentrancy is allowed. A mutex module does not itself authorize new worker sharing.
Rust similarly separates static storage from permission to share mutable state;
VOID should keep its own existing ownership rules rather than import Rust syntax.
[Rust static safety](https://doc.rust-lang.org/reference/items/static-items.html).

### GC, native resources, reload and cache

Global roots must cover strings, managed/null/interface references, nested values,
fixed/growable arrays, dictionaries and callable environments. Private/const fields,
inheritance, composition and generic specialization keep current semantics. Ordinary
source function values retain the existing private callable contract; storing one
globally does not authorize forwarded native callback registration or erase code
lease/capture-layout checks.

`assets.texture = resource` keeps its wrapper reachable, but never implicitly
calls native `deinit` when replaced or collected. Prefer explicit game `boot()` /
`shutdown()` with reached `with`/defer where applicable. On startup failure or
application close, drain tasks/callbacks and native providers while roots are still
valid, then drop global handles before Runtime destruction. Existing Runtime
destruction explicitly requires its handles to be gone. Do not call arbitrary
source global destructors after code/root teardown; no new GC finalization.

Compatible body-only reload keeps cells and values, including changes made by
older leased code. Global identity/type/visibility/readonly profile/order/default/
initializer/dependency changes require restart. Freeze the transitive source helper
bodies used by initialization using the current constructor dependency-closure
approach; otherwise a supposedly compatible body edit could silently change an
initializer that will not run again. Include conservative dynamic target families.
No generic state migration or initializer replay.

Analysis/IR and Debug cache contracts include global declarations, exact initializer
syntax, initializer helper closure, types, binding edges and ABI version. Cache
only code and immutable metadata; every fresh execution initializes fresh runtime
state. Never cache roots, open handles, cells or native addresses. Apply the same
entry prologue/guard and checks to normal/O2 JIT, cached code and linked AOT.

### Editor integration

Add one global-variable declaration category to successful and recovered compiler
records, scope lookup and reference collection. Expose type, module owner,
visibility, source docs and mutable status; use the same binding on RHS and LHS,
including compound/property/index paths. Preserve immutable snapshots and UTF-16.
Completion filters visibility; definitions/references point to canonical owners;
rename checks collisions across declarations/locals/imports/root constants and uses
the existing versioned edit guard. Standard source remains readonly; no newly unsafe
Vim rename. Failed or old overlays must not resurrect a global or publish stale
rename edits. The server never runs initialization for discovery or hover.

## Standard library review and recommended first scope

**Recommended Stage 11 core:** runtime globals plus `io`, `io/memory`,
`encoding/binary`, and a small `text` module. These solve settings/assets/save-data
and packet-building use cases without networking, reflection, a new scheduler or
process-global convenience singletons. Public APIs below are proposals.

Implement global semantics before adding library-owned shared mutable defaults.
Pure codecs/text and explicitly owned file objects do **not** technically require
globals and can be developed alongside the global frontend/Runtime work. Integrate
them after the global startup/reload contract is settled. Do not block all library
progress behind a new global subsystem, or implement the entire candidate catalog.

### File and memory I/O

| Proposed surface | Behavior and reason |
|---|---|
| `io.read_bytes(path, max_bytes = i64.max_value)` | Owned `[u8]` in a recoverable result; read to EOF, not just one short OS read |
| `io.read_text(path, max_bytes = i64.max_value)` | Same byte reader plus existing strict UTF-8 conversion; no BOM/newline rewriting |
| `io.write_bytes(path, bytes)`, `write_text(path, text)` | Create/truncate, complete-write loop, recoverable partial progress; no implicit missing-directory creation |
| `io.open_read(path)`, `open_write(path, append = false)` | Recoverable factories for opaque managed resources with `with`/one-shot deinit |
| `read_bytes(max_count)`, `read_exact(count)`, `read_line()` | Up-to vs exact vs line semantics; distinct EOF/empty line; negative counts reject |
| `write_bytes`, `write_string`, `flush`, `close` | Exact byte writes; raw UTF-8 text with no framing; flush drains owned buffers, not a durability promise |
| `read_i8/u8/i16/u16/i32/u32/i64/u64/f32/f64`, matching writes | Fixed-width codec shared with memory streams; configurable little/big endian |
| `read_string(byte_count)`, Pascal string read/write | Byte counts, strict UTF-8, framing policy below |
| `io/memory` reader/writer or stream | Owned bytes/cursor, same codec/EOF/errors; independent `to_bytes()` snapshot; no borrowed managed pointer |

Use existing result/enum/managed cleanup syntax, not C# out parameters, implicit
exceptions or a hierarchy of mandatory reader classes. File/memory adapters can
share concrete codec helpers without inventing a general iterator/stream framework.
`file.lines()` is not assumed implementable: current for iteration has no general
user-defined iterator protocol. A `read_line()` loop works without a language
extension. Whole-file functions must delegate to the same stream/read/write kernel.
[Rust whole-file reading](https://doc.rust-lang.org/std/fs/fn.read.html),
[short/exact reading](https://doc.rust-lang.org/std/io/trait.Read.html#method.read_exact).

Recommend `io.error` with operation/path, portable kind, native code, transferred
bytes, partial read bytes and cleanup failure detail. Kinds include not_found,
permission_denied, invalid_path/encoding, unexpected_eof, closed, unsupported and
io_failure. Reads distinguish `data`/`eof` in an existing-style sum value; zero-count
reads return empty data without testing EOF. Lines distinguish empty line from EOF,
strip CR/LF/CRLF and preserve lookahead in **one** input buffer. Partial data is not
lost on failure. Allocation/representability failures keep RuntimeFailure.

Preserve `console.io_error` and its canonical identity. It includes console-specific
UTF-16/stream detail and does not enumerate filesystem errors. Share private OS
error/progress handling where appropriate; do not move the public console type to
another module or add reexports/type aliases that the language does not have.

Host filesystem paths are **not VFS imports**. Recommend UTF-8 strings, Windows
wide APIs, NUL rejection, relative paths based on process cwd and ordinary host
permissions. No implicit project-root prefix, portable ASCII filename restriction,
or reuse of a source-import parser for runtime paths. Source-import exact-case,
containment and alias protections remain unchanged. Windows sharing/delete policy
must be documented; use noninheritable handles, close-on-exec on POSIX, and no shell.
Do not perform an existence check then assume a later open refers to the same file.
[CreateFileW](https://learn.microsoft.com/en-us/windows/win32/api/fileapi/nf-fileapi-createfilew),
[POSIX/Linux open](https://man7.org/linux/man-pages/man2/open.2.html).

Publish managed wrappers before exposing acquired native state; release the native
file on every failed acquisition/publication. Reuse Runtime resource admission,
one-shot close/deinit and shutdown draining. Today's `Provider` is fixture/sink
specific, so file resources need a **small explicit extension**, not an assertion
that a generic file provider already exists. Do not encode a native descriptor as
a source integer or create a second file ownership registry. Keep I/O authority
compiler-owned like console/time; importing `io` must not enable arbitrary C symbols
or consume the external fixture provider slot. Copy bytes before NativeSafe I/O;
never hold a GC payload borrow across an OS wait.

Writes are not transactional: truncate/append may leave a modified file after an
error. Do not promise atomic save replacement or disk durability from `write_text`.
An explicit future atomic-write API may extract appropriate existing publication
primitives, but must not reuse executable PE checks or the entire native linker
staging system. Report cleanup errors without replacing the primary I/O error.

### One binary codec and explicit framing

`use encoding/binary as binary` selects `stdlib/encoding/binary.void`; it does not
create an `encoding.binary` namespace object. Recommend fixed-width typed encode/
decode helpers, reusing existing generic specialization only where it actually
reduces wrappers. No ordinary function overloading or primitive extension-method
system is implied. Decode exact widths; reject trailing/short buffers. Float codecs
preserve IEEE bits, including signed zero/NaN payload, instead of numeric casts.

Recommend little endian by default, explicit big endian; never native-endian
defaults in portable assets/packets. Raw `write_string` writes UTF-8 bytes only.
Pascal string format: u32 **byte** length in the selected byte order, then exactly
those bytes, no NUL/terminator/newline. Reject u32 overflow before writing a prefix;
validate truncation/UTF-8 on read. Caller `max_bytes` can bound untrusted frames;
do not replace the existing representability policy with an arbitrary global cap.
Check available file/memory extent where known before allocating from a prefix.
Prefix/payload failure retains precise partial progress; no rollback promise.
Godot is inspiration, not a wire-compatibility claim: its string framing includes
different termination details. [Godot StreamPeer](https://docs.godotengine.org/en/stable/classes/class_streampeer.html#class-streampeer-method-put-string).

Memory streams have one cursor and managed identity; aliases share position. Copies
of `to_bytes()` do not. Recommend seeking only within existing memory extent;
appending grows with checked arithmetic, never creates sparse uninitialized gaps.
File offsets use checked i64 positions and native offset APIs. Share byte/numeric
semantics with file streams; allocation and publication use existing arrays/GC.

### Async boundary: a real scope decision

Boss favors paired blocking/`_async` names. Those remain the direction. They are
**not trivial wrappers**: async source functions currently run on the owning
executor, workers run restricted source frames, native resources cannot simply
cross task/worker boundaries, and the CLI does not select async main.

Recommend a complete blocking initial I/O surface for this small stage, with async
file APIs a separately approved follow-on **rather than names backed by blocking
owner execution**. Boss must explicitly accept that initial scope (D4 below).
Do not document async I/O as delivered or make it a hidden engineering deferral.

If paired async I/O is required in Stage 11, add one concrete dependency to its
approved scope: native file-job admission/completion on the **existing bounded
executor**, using owned path/byte buffers, NativeSafe OS work and owner-thread
publication into existing rooted tasks. No thread per request; no new task model.
Begin with whole-file operations returning bytes/text/results so no file resource
crosses a public task result. Cancellation must retain buffers, handles and code
leases until actual completion; it cannot undo already written bytes or guarantee
immediate cancellation of every filesystem syscall. File-object async methods and
async open require a further explicit resource-lifetime/admission policy. An
`async func` calling a sync function does not satisfy this requirement.
[Async I/O thread offloading precedent](https://docs.python.org/3/library/asyncio-task.html#asyncio.to_thread).

### Other candidate modules: reviewed, not silently included

| Candidate | Recommendation / dependencies |
|---|---|
| `text` | Include small literal `starts_with`, `ends_with`, `split`, `join`, `replace`, scalar substring helpers. Reuse strict UTF-8/builder; no byte-index confusion, regex dependency or repeated O(n²) concatenation. Empty separator and empty replacement pattern need explicit rules; recommend reject empty split/replace patterns. No implicit locale/grapheme promises |
| `console` | Keep print/end/file/flush and existing read outcomes; a prompt/input convenience can be added later once its EOF/error result is selected. Never add another input buffer or duplicate print |
| `convert` | Useful next: exact typed number/bool text parsing, recoverable errors, strict entire-input consumption. Reuse numeric policy, not unsafe numeric `as`. Do not claim primitive `.parse<T>()` or enum metadata already exists |
| Remaining `encoding` | UTF-8 already exists; thin wrappers only if valuable. Add strict ASCII/Base64/hex later with one decoder each, not overlapping roots and nested facades |
| `random` | Next gameplay slice: explicit seeded generator first, default runtime generator only after globals semantics. Distinguish exclusive bounds from inclusive integer range; unbiased rejection sampling, full-width overflow cases, floats [min,max), secure RNG separate. A 0/1/2-argument overload family is not currently legal ordinary source. PCG is a viable small reproducible candidate, not selected implementation. [PCG bounded generator](https://www.pcg-random.org/using-pcg-c-basic.html) |
| `math` | Existing numeric abs/clamp/limits stay canonical. Next additions min/max/lerp/trig need exact f32/f64/integer/NaN rules; no implicit widening or Math/MathF duplicate module families |
| `net/http`, `net/tcp`, `net/udp` | Later; reuse bytes/codec/resource core. Keep protocol identities separate. DNS/TLS/timeouts/cancellation and real async completion are dependencies, not automatically provided by io |
| `sync` | Later explicit sharing design. Scoped locks reuse managed deinit and NativeSafe waits; no lock across await by accident. A mutex cannot authorize mutable globals in current workers. Atomics/semaphores need memory model, ownership and cancellation policy |
| `channel` | Instantiated typed pub/sub router, not singleton, typed event replacement or worker queue. Recommend homogeneous `router<T>` for string topics first; mixed payloads require explicit typed topic IDs/tagged sum, not hidden reflection. Reuse synchronous event snapshot/order/removal semantics; owner-thread dispatch, explicit unsubscribe tokens/lifetimes and reentrancy policy. Strong subscriptions can retain whole game subsystems |
| `log` | Later small module over existing formatting/output. Arguments/interpolation are evaluated before an ordinary call, so `log.debug("{expensive()}")` cannot avoid formatting just by checking level inside the function. Use an explicit enabled check or later ordinary lazy callback; no new logging compiler. [slog performance guidance](https://pkg.go.dev/log/slog#hdr-Performance_considerations) |
| `path`, `os` | Later pure host path helpers, app data/config/cache and real bundle detection. Keep separate from VFS; do not equate cwd/executable/project paths |
| `json`/CSV/TOML/XML | Later focused parsers; JSON dynamic AST can work without reflection. Arbitrary typed serialization needs explicit converters or separately approved compiler metadata |
| Collections | Builtin get/pop/count/copy/version rules already exist. Sort/map/filter later with comparator/lambda/readonly/order contracts; no new mandatory List/Queue/LINQ hierarchy |
| `process`, `args` | Later argv-safe subprocesses and schema-based CLI parsing. Raw `[string]` main support requires a real entry change; not provided by an args library |
| `compression`, `crypto`, `checksum`, `regex` | Later shared byte/stream providers, bounded outputs and explicit security/resource policies. Crypto is not gameplay RNG; checksum is not authentication; literal text operations need no regex |
| `tasks`, `time` | Reuse current temporal values, clocks, async groups/cancellation. Delay needs scheduler timer support; no new thread per call or second time library |
| enum/flags, testing/bench, debugger | New declaration/CLI/bitwise/debugger contracts, not ordinary stdlib files. Debugger execution needs more than LSP. Do not smuggle them into this first scope |
| database/url/system; general reflection | First group remains backlog; general reflection/Activator was withdrawn and is excluded |

For `text`, recommend scalar substring start/count (nonnegative, bounds-checked);
split preserves empty fields and join preserves order; replacement is left-to-right
nonoverlapping. Case conversion/Unicode whitespace/grapheme operations need pinned
Unicode semantics and can remain outside the initial literal-text subset. This is
an explicit proposed subset, not a claim of a complete text-processing library.

## Boss decisions required before engineering

| ID | Decision | Recommendation |
|---|---|---|
| D1 | Application state lifetime and concurrent access | Once per admitted program Runtime, repeated calls/reload retain state, fresh program gets fresh state; owner-thread global access; reject incompatible Runtime reuse |
| D2 | Init effects, deterministic order, cycles, failure and entry migration | Eager reachable modules; import-component order then lexical vars; guarded unready access; synchronous effects but no task/callback admission; failed state never retries; hoist global initializers before script statements, allow globals + main/legacy |
| D3 | Root globals and visibility | Normal own-file/qualified imports, public/internal/private; no implicit root-global lookup; unchanged root const and lexical no-shadowing |
| D4 | Initial library scope and async obligation | io/io-memory + binary codec + small literal text surface; blocking I/O first. If Boss requires paired async now, approve the native-job dependency explicitly before Keyboard starts |
| D5 | File/packet observable behavior | Recoverable domain error with partial progress; cwd-relative host paths; raw UTF-8 strings; u32 Pascal byte prefix, selected byte order with little default; nontransactional write semantics and caller budgets |

Wrench can refine implementation choices without reopening settled GC/ownership.
These recommendations do not approve themselves. Avoid an owner questionnaire for
routine helper factoring, but do not hide the script migration or async omission.

## Main risks and evidence limits

1. Global lookup is easy to parse but hard to make safe across generation retirement,
   partial startup, value/reference copies and foreign callbacks. No code addresses
   or thread-local persistent frames should become global roots.
2. Existing default Runtime sharing is a concrete isolation hazard for any new
   per-Runtime global table; names/hash equality are not application identity.
3. Initializer side effects and entry hoisting are observable behavior changes.
   Write the contract and migration tests before optimizing or adding convenient
   implicit visibility.
4. Native resource helpers currently embody fixture/sink operations; file I/O needs
   real provider/lifetime work, despite reusable root/cleanup mechanisms.
5. Async APIs require actual suspension/completion and ownership work. Source
   `_async` spelling alone is no evidence of nonblocking behavior.
6. Runtime file paths and import VFS paths have different purposes. Reusing the
   entire import resolver would reject ordinary user files; weakening it would
   damage import security. Reuse encoding/platform primitives at the correct layer.
7. This research ran only the eight bounded local behavior controls above and
   documentation checks. It did not execute the locked hosted binaries, new
   platform I/O probes, new globals, a full compiler suite or hosted CI.

All new behavior, resource failure paths and platform claims require the focused
[roadmap acceptance](STAGE11_ROADMAP.md). No production implementation, package
manager, branch, commit, push, CI dispatch or root README change is included.

Documentation verification: both requested files have balanced fences, no trailing
whitespace and 24 valid local link targets. Repository-wide `git diff --check`
initially passed; a later check reported trailing whitespace at
`stdlib/console.void:428` in the separately preserved concurrent edit. No repository-
wide clean-diff claim is made. Eight local behavior controls produced their expected
outcomes; no production test suite, build or hosted CI was run. The worktree was not
reset or cleaned.
