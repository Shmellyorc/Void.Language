# Runtime globals — Stage 11

**STAGE 11: OFFICIALLY LOCKED, October 25, 2026.** Golden Wrench reviewed and
accepted the implementation; Boss authorized the lock at tested commit
`e8633b246fe6139d39d830afd4cb249f7dab4d8e`.
[Final CI 40248573233](https://github.com/Shmellyorc/Void.Language/actions/runs/40248573233),
`ci-link-experiment.yml`, `mode=thinlto-cache`, passed **7/7 jobs on attempt 1,
1,407/1,407 CTests**, with hosted Debug editor integration on Linux, Windows and
macOS. [Official acceptance](stage11/ACCEPTANCE.md) records all platform counts,
the separate local Linux 609/609 result and acceptance exclusions.

Boss authorized complete file-level runtime globals on 25 October 2026. This
contract supersedes the visibility/startup recommendations in the Stage 11 research
and roadmap. [The Stage 11 handoff](stage11/IMPLEMENTATION.md) describes the
implementation. The earlier locally reproduced LLVM 19.1.7 DSE crash remains
unexplained: subsequent runs passed without a demonstrated repair, and it did
not recur in hosted acceptance. The separate local ThinLTO cache/link incident
does not establish a DSE fix. [The original observation](stage11/OPTIMIZER_FAILURE.md)
and historical evidence remain preserved. Compiler-owned hover/documentation
repair and standard-library expansion remain future work; the lock authorizes no
next engineering stage.

## Declarations and visibility

Outermost `var` declares shared runtime storage in every reachable `.void` module.
Function, method, conditional, loop and other block bindings remain lexical.
Fields and compile-time module `const` keep their existing meanings. There is no
`global` or `nonlocal` keyword. Existing no-shadowing and safe-default rules apply.
A nonnullable managed reference or function requires an initializer. Runtime
globals are unavailable to compile-time constants, field defaults and parameter
defaults. Type-inference cycles require an explicit annotation; annotations do not
make an uninitialized runtime value readable.

The selected entry file defines the project-root globals. Its public/internal
globals are directly visible to other project modules, just like root constants.
An explicitly selected filename has the same role as discovered `main.void`.
Own module declarations retain their precedence over unqualified imports. Type
constructors keep their ordinary context, including generic type parameters;
conflicting imported values and visible root constants diagnose ambiguity.
Private globals remain file-private. Ordinary module globals follow the existing
unqualified import, explicit alias, qualified access and public/internal/private
rules. Standard-library and project visibility domains remain separate. Different
imports/aliases of the same module always access the same storage.

```void
# main.void
use "./game.void" as game
var score = 0
func main():
    game.reward_player()
    print(score, game.enemies_defeated)

# game.void
var enemies_defeated = 0
func reward_player():
    score += 10
    enemies_defeated += 1
```

[Runnable example](../examples/globals/main.void) calls the reward twice.
Ordinary value/fixed-array reads and assignments preserve independent snapshots;
managed, list and dictionary references retain identity. Deep readonly authority
survives globals, copies, helpers, aggregate fields and returns. Rebinding a global
does not implicitly release a native resource or run a source destructor.

## Startup and failure

Checking, tooling, lowering and cache population never execute initializers.
Execution initializes every reachable global eagerly before the selected entry.
Modules initialize dependency-first. Import strongly connected components remain
legal; members of a component use canonical VFS order. Ready component ties use
canonical order. Declarations within each module initialize in source order.
Unimported modules do not initialize. Function-only cycles and independent global
cycles are legal.

All outermost initializers execute before remaining top-level script statements,
even when interleaved in source. For example, `print("before")` followed by
`var item = make_item()` evaluates `make_item()` before that print. A local binding
inside a function or block preserves ordinary execution order. Globals coexist
with `main` and legacy `entry`; actual script statements retain the existing
entry competition rules. A selected global-only file initializes and exits zero.

Initializers may use ordinary synchronous expressions and functions under existing
safety/effect rules. Task startup and native callback registration are forbidden
through initializer dependencies. An ordinary stored `Err` is data. Bare `?` has
no ambient result-returning context. Entry discovery and synchronous main
signatures are unchanged.

Reads and writes of globals whose cells have not been published fail with an owned
runtime diagnostic and source trace; no default/partial value is exposed. Helpers
and cycles receive the same check. Failure prevents later initialization and entry
execution, drains acquired native resources, and remains failed on later invocation.
Earlier external effects are not rolled back or replayed. Restart creates a fresh
application instance.

## Runtime, concurrency and lifecycle

Each global is one complete managed cell traced by the existing precise Heap,
held through a Runtime-owned persistent Handle. Root slots exist before source
initializers run. No live data, native address or JIT-owned callback is serialized
or retained in a layout descriptor. Scalar-only global programs use this same
coordinated Runtime path.

Repeated calls retain initialized values. Independent JIT compilations and cache
hits get distinct application Runtimes. Explicit host-supplied Runtimes admit one
global program; compatible replacement reuses that admission. Matching VFS names
or hashes do not authorize independent programs to share state.

Global access belongs to the initializing application thread. Worker-reachable
reads and writes reject through the existing transitive effect validation and
independent IR validation. The owner may evaluate a safe value snapshot and pass
it to a worker. Owner async activations retain ordinary interleaving; globals are
not implicit atomics. Foreign host/callback accesses do not gain owner authority.

Native resources preserve explicit `with`/`defer`/`deinit`/provider cleanup rules.
Failed startup drains native providers before releasing global handles. Runtime
close likewise drains providers while roots are valid, then releases handles.
The collector does not run source destructors.

## Reload, cache and tooling

Compatible function-body replacement preserves all cells and values. Global
identity/type/visibility, declaration order, initializer syntax, layout/readonly
profiles and import dependencies require restart when changed. Source helper
bodies used by initialization participate in the existing frozen initialization
closure contract. Dynamic calls require conservative dependency closure. Old
leased generations continue to address the same Runtime cells.

Debug cache artifacts contain code and immutable metadata only. A hit binds the
same guarded Runtime ABI and receives fresh state for a new application. JIT,
O2 JIT and linked AOT share typed IR and LLVM lowering; AOT admits metadata through
the existing Runtime entry bootstrap before initialization.

Globals have compiler-owned declaration/reference identities for completion,
hover, definition, references and guarded rename, including import aliases and
root visibility. Unsaved overlays and stale-result rejection keep the existing
language service contract. Installed standard sources remain protected. Editors
do not execute startup or maintain a separate semantic parser. The separate
result/Ok/Err/parameter-documentation hover repair is outside this assignment.
