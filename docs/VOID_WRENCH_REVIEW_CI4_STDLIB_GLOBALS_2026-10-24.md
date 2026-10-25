# VOID 2.0 Golden Wrench Review: CI #4, Standard Library, and File-Level `var`

**Date:** 24 October 2026  
**Role:** Independent Wrench Clanker architecture and evidence review  
**Status:** Review and recommendations only. Not a Boss-approved language contract or implementation authorization.  
**Sources:** Uploaded `WRENCH_CLANKER.md`, uploaded standard-library/global-variable handoff, and current `Shmellyorc/Void.Language` source and docs on GitHub (read-only inspection).

## 1. Independently verified ThinLTO experiment #4

- [Compiler link experiment #4](https://github.com/Shmellyorc/Void.Language/actions/runs/40118666209), `workflow_dispatch`, **successful**, 7/7 completed jobs at `90c0cf64b9aba60d7c642096d2aaac45aae70d56`.
- Created 2026-10-24 18:00:03 UTC; updated/completed by 19:08:47 UTC. Overall wall interval approximately **1 h 8 m 44 s**.

| Hosted configuration | CTest result | Job |
| --- | ---: | --- |
| Ubuntu Debug | 199/199 | Success |
| Ubuntu Release | 199/199 | Success |
| Ubuntu strict sanitizers | 199/199 | Success |
| Windows 2022 Debug | 194/194 | Success |
| Windows 2022 Release | 194/194 | Success |
| macOS ARM64 Debug | 199/199 | Success |
| macOS ARM64 Release | 199/199 | Success |
| **Total** | **1,383/1,383** | **7/7** |

The previous macOS Release console test failures are corrected in this experiment: both `runtime_console_native_terminal` and `runtime_console_native_transfers` passed in the actual Release log. Both also passed on macOS Debug and Ubuntu. Linux strict sanitizers and the frontend gate passed, and real VS Code editor integrations passed in the applicable Debug jobs on Linux, Windows, and macOS. The Linux Debug job also ran the separate frontend small-stack CTest. This does not claim that optional unexecuted clients or a second physical Windows volume were covered.

This is **strong experimental native-platform evidence**. It is **not** evidence that the original main CI workflow was modified or separately passed, or that Stage 10's Steps 43 and 44 are complete. The previous main CI #98 and experiment #3 failures remain historical facts, not current failures in experiment #4. Their separate fixture synchronization causes and repairs are recorded in [`docs/stage10/engineering/ci98/DIAGNOSIS.md`](https://github.com/Shmellyorc/Void.Language/blob/main/docs/stage10/engineering/ci98/DIAGNOSIS.md); no production Runtime repair is established by that diagnosis.

`main` currently points at `92136acaf114a492452a7d84b31f174f5b305def`, precisely one commit after tested `90c0cf64...`. The later commit adds only `docs/VOID_STDLIB_AND_GLOBAL_VARS_WRENCH_HANDOFF_2026-10-24.md` (550 new lines), with no source/test/workflow change. Keep the tested SHA tied to the correct evidence.

**Decision:** Accept CI #4 as green experimental cross-platform evidence. Have Keyboard reconcile Step 42 native obligations and document exactly what remains in Step 43/44. Do not run another seven-job CI solely to update documents. Only Boss can decide whether and when to adopt the alternative linker configuration into standard CI.

## 2. Wrench succession handoff: status and governance

The uploaded Wrench handoff is internally historical: its opening milestone says CI #95 Windows resolved, and its last Stage 10 append describes incomplete engineering around October 22. Neither is the current last tested milestone. The file also preserves older, superseded recommendations about normal-chat versus Work. **Retain history but put a fresh, short, dated current-status pointer at its beginning** so future Clankers do not misread archival statements as active restrictions.

The standing rules are sound: Boss alone controls major design, commits, pushes, branch operations and hosted CI; Wrench independently reviews; Research investigates; Keyboard implements only authorized scope and reports minimal output. Preserve KISS/DRY, one frontend and typed IR, LLVM 19.1.7, shared JIT/AOT, precise GC, hot-reload generation leases, native evidence distinctions, existing tests, the protected root README and internal docs under `docs/`.

## 3. What already exists, so the library must not duplicate it

GitHub presently has `stdlib/console.void` and `stdlib/time.void`. The compiler provides source `print`, `string(value)` and recoverable `format(value, spec)`, heterogeneous display and temporal features from Stage 10, plus the Stage 8 numeric prelude (`T.abs` / `.abs`, `.clamp`, numeric limits) and UTF-8 text conversions (`string.from_utf8(bytes)?`, `text.to_utf8()`), scalar length/indexing, recoverable list/fixed `get`, dictionary `get`, and **list `pop()`**. Some Stage 10 final acceptance is still pending despite substantial working functionality. Use their existing contracts, not parallel implementations.

Import-layout correction: the **current official standard-library resolver** selects an **exact `.void` file path** (`use io/files` -> `stdlib/io/files.void`). It does **not** use the `libs/` repeated-folder entry convention. Thus the proposed `stdlib/sync/sync.void  # use sync` mapping does not follow current standard import rules. The low-cost compatible layout is `stdlib/sync.void` for `use sync`, plus `stdlib/sync/guard.void`, `stdlib/sync/mutex.void`, etc., for `use sync/guard` and `use sync/mutex`. Do not add a general new directory index resolver just to make the example work.

The global-assets pseudocode uses `if hero_texture is Ok(var tex)` on a `texture?` **nullable**, not a `result<texture,e>`. This is a sample type mismatch, not an engine bug. Rewrite it using the actual supported nullable test/unwrapping form after consulting current source and tests. `texture` itself is expressly only a placeholder.

The report's claim that `main(raw: [string])` works is **not currently established** by the published Stage 9 application-entry contract: that contract explicitly describes zero-argument application main and rejects app operands. Boss reports later main parameters; have Keyboard inspect actual source and execute a focused test before settling or updating that statement. Do not overrule Boss's report from stale docs or declare it implemented without executable evidence.

## 4. Full module-by-module architecture assessment

| Candidate | Wrench assessment | Open boundary |
| --- | --- | --- |
| `console` | Add a tiny high-level prompt/read wrapper over present standard endpoints; do not duplicate built-in `print()` or own a second input queue. | `input()` result, EOF, redirected prompts, and failure policy. |
| `convert` | Text parsing to typed numbers/bools/enums is valuable, distinct from binary codecs. Reuse existing display/format and numeric parsing policy. | Ordinary function overloading and `"42".parse<i32>()` extension syntax are not generally available by implication. |
| `encoding` | Good separation of UTF-8/ASCII/Base64/hex and fixed-width binary primitives. Reuse existing `to_utf8` / `from_utf8` and share binary codec with I/O. | Exact byte order, strict error behavior, ASCII policy, width, lifetime of buffers. |
| `io` | High utility: whole-file helpers plus streaming files, memory streams and exact primitive readers/writers using one codec. | EOF/partial errors, scoped ownership, size limits, `_async` semantics; raw vs Pascal string frame width and endianness. |
| `random` | Keep **exclusive** upper-bound `rand_i32(...)` and **inclusive** `rand_range(...)` as distinct intents. Provide seedable generators separately from secure randomness. | Zero/negative bounds, integer max/overflow, unbiased distribution, float endpoint policy; the proposed 0/1/2-argument `rand_i32` family is not automatically supported by current ordinary-function overloading. |
| `math` | Useful for min/max/lerp/trig, but `.abs()` and `.clamp()` already exist on numeric values. Wrap or reuse rather than reimplement. | Typed interpolation and coercion, f32/f64 and integer `lerp`, NaN ordering. |
| `text` | Trim, split, join, replace, starts/ends-with are valuable. Preserve existing scalar indexing/length and bytes separation. | Unicode scalar vs grapheme semantics, locale-sensitive case operations, substring unit and negative indices. |
| `enum` and `flags enum` | Typed enum names/values may be possible via compile-time generation without general runtime reflection. Typed bit flags are a valid separate design. | `flags enum` **adds syntax and bitwise operator semantics**, not merely a library; custom `has` must not collide with `Type has`; zero/composites/mutation open. |
| JSON/CSV/TOML/XML | Separate small data modules rather than a giant serializer. Dynamic JSON AST can work without type reflection. | Automatic `json.parse<t>` into arbitrary structs needs explicit serializers or approved compiler-derived metadata; no general reflection is authorized. Typed-vs-dynamic overload spelling needs design. |
| Collection algorithms | Add sort/find/map/filter selectively to built-in `[T]`/fixed arrays; no LINQ clone or new mandatory List/Queue containers. | Comparator type inference, ordering consistency, mutation/readonly/version effects, stability and allocations. `pop()` already exists. |
| `net/http`, `net/tcp`, `net/udp` | Correctly separate protocols and reuse byte arrays/stream codecs. HTTP convenience should hide socket detail without erasing explicit TCP/UDP behavior. | TLS, DNS, timeouts, message vs byte boundaries, native security and `_async` cancellation. |
| `path`, `os` | Keep pure path manipulation separate from app data/config/cache directories, environment and macOS bundles; use native conventions. | Filesystem path versus VFS import path, sandbox/permission failures, real bundle detection, newline policy. |
| `compression` | Offer byte and streaming interfaces over shared codecs; gzip/zlib first; ZIP is an archive abstraction. | Bomb/output budgets, error handling, stream lifetime; no full-file buffering assumption. |
| `log` | Simple logging API over already implemented display with level-aware early gating. | Structured fields and deferred formatting only when justified; do not build a logging compiler. |
| `process`, `args` | Plain `[string]` raw arguments plus optional parsing is the right goal, not a special process argument object. Subprocess should use argv rather than interpolation. | Verify **actual** `main` signature support; parsing needs option schema to resolve ambiguous flags/values. |
| `crypto`, `checksum` | Distinct APIs for cryptographic integrity/randomness and accidental corruption. | Legacy MD5 is not a secure digest; platform-backed RNG and safe failure policy. |
| `testing`, `bench` | Valuable later but the proposed `test func` and `bench func`, plus `voidc test`/`bench`, are **language/compiler/CLI** changes, not stdlib-only features. | Test discovery, production exclusion, metrics provenance, CI integration. |
| `regex` | Optional compiled regex for advanced cases; ordinary string methods must remain enough for normal use. | Unicode model, bounded work for untrusted expressions, error/reporting surface. |
| `sync` | Scoped `with` guard is intuitive, but it must use actual managed `deinit` and lock ownership semantics. | Worker-shared mutable state is currently restricted; ordinary mutexes cannot silently lift transfer restrictions or safely cross `await`. |
| `tasks`, `time` | Reuse existing async executor, task groups, cancellation and **already implemented** `time` temporal APIs. | Delay scheduling without a new thread per call; clock choice and cancellation; explicit `_async` where appropriate. |
| `channel` | Distinct, instantiated pub/sub router, **not** a global singleton, worker queue or replacement for typed `event`. | Topic/payload typing, callback storage and lifetime, reentrancy/ordering, thread and GC rules. Heterogeneous string topics imply a type policy; avoid covert reflection. |
| `debug` | Basic asserts/traces; defer `debug.breakpoint()` until source debugger integration is real. | DAP/editor debugger support is not supplied by LSP alone. |
| Lower-priority `database`, `url`, `system` | Keep in backlog only; no approved interface yet. | General runtime reflection/Activator-style creation was expressly withdrawn and is excluded. |

## 5. File-level runtime `var` in any `.void` module: recommended minimum contract

**Owner-preferred visible grammar:** Outer file-level `var` declares one mutable runtime binding; function, loop, condition and struct member `var` retain their current lexical/field meaning; no `global` or `nonlocal` keyword. This is a **proposed new semantic feature**, not today's deployed global-variable behavior.

A coherent implementation contract should have the following invariants:

1. **Canonical identity/storage:** exactly one global storage location per canonical module + symbol identity **per application Runtime instance**, regardless of diamond imports, aliases or multiple calls. A new invocation gets new state. Qualified access uses the same cell; mutable value struct loads/copies continue obeying value semantics.
2. **Entry scripts:** distinguish top-level executable statements (still only selected entry) from file-level global declarations (permitted in any module). The current selected-entry script's top-level `var` is local to a synthetic body. Making it global changes observable timing and visibility. Explicitly specify that migration and test interleaved expressions/statements before claiming backward compatibility.
3. **Initialization:** safe defaults or explicit initializers, no uninitialized nonnullable references; init **once** per Runtime graph in deterministic order before observed reads. Do not run arbitrary imported top-level statements. Cycles and initializer failure must diagnose with owners; do not require import-cycle rejection solely because function import cycles exist. Use existing dependency/effect analysis and a bounded runtime initializing/initialized state only if the simpler static analysis cannot settle an actual cycle.
4. **Name lookup:** own-file globals unqualified; normal direct `use` aliases/access/public/internal/private apply across non-root modules. One **Boss decision** remains: should public/internal globals of the selected `main.void` also inherit project-wide implicit visibility like its compile-time root constants? Favor preserving the established root precedent for simplicity only if collision and initializer-dependency diagnostics remain deterministic; do not silently choose it.
5. **GC and native resources:** Runtime-owned precisely scanned global roots for strings, managed references, nullable/interface values, arrays, dictionaries and nested values. Ensure normal/O2 JIT, linked AOT, safepoints and shutdown. A rooted texture wrapper does not automatically release an OS/GPU handle. `deinit`/explicit shutdown is a separate lifetime requirement.
6. **Worker isolation:** mutable globals are **not** automatically shared safely between Stage 6 workers. Source and independent IR must preserve current transfer/effect restrictions; synchronization work is a separate approved contract.
7. **Reload/cache:** preserve global identity/value during compatible body-only reload; declaration, type/layout, initializer/dependency and unsafe policy changes require an explicit restart/invalidation contract. No replay of initializers during ordinary body hot reload, and no raw generation pointer retained as a root.
8. **Tooling/tests:** one compiler-owned declaration/scope graph; test cross-file visibility/collisions, lexical branches, import diamonds/cycles, init order/failures, GC retention/cleanup, readonly authority, worker rejection, reload/cache and source/IR/JIT/O2/AOT; real editor lookup and rename/recovery in the same snapshot model.

**Real owner decisions still required** before this becomes an approved implementation specification: (a) global initializer effects and whether all reachable modules are initialized eagerly, (b) global init order plus interleaved entry-script behavior, and (c) main-file globals' optional implicit project-wide visibility. Do not bury these questions inside a Keyboard coding task. Wrench recommendation: prefer eager, deterministic one-time initialization with a clear migration rule, no arbitrary imported executable statements; use current VFS/public visibility unless Boss chooses the existing root-constant special case.

## 6. Safe next sequence

1. **Now, documentation/evidence only:** record CI experiment #4 under Stage 10 Step 42, replace stale *current-status headers* in `docs/stage10/IMPLEMENTATION.md` and `docs/WRENCH_CLANKER.md` with dated facts, and preserve CI #98/experiment #3 failure investigations unaltered. Review remaining Step 42 native evidence, Step 43 docs/examples and Step 44 final Wrench/Boss gate. No need to repeat seven hosted jobs for docs alone.
2. **Finish Stage 10's already authorized 44-step acceptance.** Treat CI #4 as green experiment evidence for the cross-platform matrix, not a magic Stage 10 lock. Document actual Windows/macOS scope and any remaining exclusions.
3. **Design approval for file-level `var`** as a coherent language feature, with Boss selecting the open semantics above, then one authorized engineering delivery and focused source/runtime/tooling acceptance.
4. **Later stdlib implementation** can proceed in dependency-friendly complete slices (small console/convert/text conveniences, then encoding/io/memory-stream core, then random/math/data, networking/compression etc.). This is an advisory sequence, not newly granted implementation authority or an arbitrary mandatory gate. More invasive language features (`flags enum`, test/bench declarations, worker-safe synchronization) need their own semantics decision.

### Keyboard Clanker instructions to record now

**Documentation-only mission.** Read `docs/README.md` first, then the current Wrench and Stage 10 records, CI #98 diagnosis, and the proposed `VOID_STDLIB_AND_GLOBAL_VARS_WRENCH_HANDOFF_2026-10-24.md`. Inspect CI #4's native job logs and artifact metadata. Update under `docs/`:

- Add the precise CI #4 run URL, tested commit SHA, 7/7 jobs, **1,383/1,383** CTest totals (five times 199 and two times 194), 1 h 8 m 44 s run interval and named macOS Release fixture passes to `stage10/IMPLEMENTATION.md`, Step 42 evidence/results, `WORK_LOG.md`, `WRENCH_CLANKER.md` and the docs index where relevant. Preserve earlier CI #98 / experiment #3 6/7 results, root README and original CI workflow.
- Record Step 42 as **green native experimental evidence with formal reconciliation pending**, and Step 43/44 as pending until their actual acceptance is performed. Clearly separate a tested experimental run from a main-CI lock; report any platform obligations that were not covered.
- Preserve the stdlib/global handoff as **design only**, either append a short correction note or prepare a separate review report. Note the standard import layout mismatch (`stdlib/sync.void` rather than `stdlib/sync/sync.void` for `use sync`), currently implemented `pop`, `to_utf8`/`from_utf8`, numeric prelude, `console`/`time` and Stage 10 display APIs; flag the nullable-vs-`Ok` example and verify `main([string])` in actual source/tests before documenting it as supported.
- List the genuine Boss approval questions about file-global initialization, entry-script ordering, visibility and typed channel topics. **Do not implement globals/stdlib, change runtime/compiler, alter CI, commit, push, branch, trigger hosted CI or edit root README.** Preserve tests and exact evidence. Report only docs changed, evidence recorded and remaining owner decisions.

**Workshop rule:** Three Clankers. One VOID. Strong bolts. No ghost bolts.
