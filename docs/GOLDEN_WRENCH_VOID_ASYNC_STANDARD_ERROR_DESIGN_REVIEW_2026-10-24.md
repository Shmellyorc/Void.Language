# 🔧🤖 Golden Wrench Clanker — VOID Async Syntax and Standard Error Review

**Date:** 24 October 2026 (Boss local date)  
**Requested by:** 👑 Boss; Silver Wrench Clanker design handoff  
**Review status:** Independent architecture review **COMPLETE**; proposed features **NOT IMPLEMENTED OR AUTHORIZED FOR IMPLEMENTATION**  
**Permanent intended repository path:** `docs/GOLDEN_WRENCH_VOID_ASYNC_STANDARD_ERROR_DESIGN_REVIEW_2026-10-24.md`  
**Source inspected:** `Shmellyorc/Void.Language`, `383d2ce0a404a4bbf3df01d43097de100f5829a1` (read-only published main). Any later local uncommitted changes were not available for this review.  
**Scope:** documentation/design only. No compiler, Runtime, tests, CI, README, Git, or hosted execution changed.

## Decision and executive review

**GO as a future design direction; HOLD on implementation until separate Boss authorization and resolution of the two remaining language-API choices.** Silver Wrench's proposed standard error shape is compatible with managed single inheritance and generic results. A narrowly scoped syntax adjustment for `await job?` appears feasible within the existing frontend, with no second scheduler or new `result` engine.

**One important independent finding:** the exact matching policy for `?` and the current source/IR type checks have a possible mismatch when an error payload is a managed derived type. This must be reproduced with a targeted compiler regression before any claim that derived-to-base result propagation is either accepted or rejected today. Preserve the Boss-approved exact-error contract; do not silently broaden it.

Stage 10 remains **INCOMPLETE** pending separate native acceptance/release decisions. These proposed features are **not** Stage 10 additions under the currently approved 44-step scope. Do not open Stage 11 or implement based on this review alone.

## A. Boss-approved design direction, preserved exactly

A common VOID standard error type shall have **exactly these initial three fields and types**:

```void
struct error:
    message: string
    path: string
    code: i32
```

- `message`: human-readable explanation.
- `path`: relevant resource/location, such as a file, asset, module or configuration path; not restricted to filesystems.
- `code`: signed 32-bit numeric identifier, defined by the producing API; **no universal code registry**.
- Specialized managed errors should be possible via existing single managed inheritance:

```void
struct network_error extends error:
    status: i32
    address: string
```

- `result<T,E>`, contextual `Ok`/`Err`, and `?` remain the canonical recoverable-error mechanism. Existing `result<T,string>` and other specialized error types remain legitimate.
- Unexpected runtime failures and task cancellation are distinct from `Err`; adding standard `error` must not convert either automatically.
- Preserve **exact matching of propagated error types**. No implicit `result<T, Derived>` → `result<T, Base>` widening; no exception framework, mandatory stack/cause/timestamp fields, implicit panic conversions, code registry, or new object model.
- Boss prefers `await job?` to mean **`(await job)?`**, with `await` before result propagation, while retaining cancellation, structured draining, defer/cleanup, GC and JIT/AOT behavior.

**Not additionally approved:** field default spellings/mandatory-message validation; prelude exposure; a `string()` display hook; implementation schedule; any change to derived-result exact typing.

## B. Source-grounded review of `await job?`

### Current behavior (confirmed statically)

At [compiler.cpp L829–L859](https://github.com/Shmellyorc/Void.Language/blob/383d2ce0a404a4bbf3df01d43097de100f5829a1/src/frontend/compiler.cpp#L829-L859), `postfix()` greedily consumes `?` and wraps its operand in `Expr::Kind::propagate`. At [L901–L915](https://github.com/Shmellyorc/Void.Language/blob/383d2ce0a404a4bbf3df01d43097de100f5829a1/src/frontend/compiler.cpp#L901-L915), the prefix `await` parser recursively consumes a `unary()` operand. Thus:

```void
await job?
```

currently builds an AST equivalent to **`await (job?)`**, not **`(await job)?`**. With `job` an inferred task handle, `?` expects a `result` but receives a task. [The result check](https://github.com/Shmellyorc/Void.Language/blob/383d2ce0a404a4bbf3df01d43097de100f5829a1/src/frontend/compiler.cpp#L2244-L2249) and [the await check](https://github.com/Shmellyorc/Void.Language/blob/383d2ce0a404a4bbf3df01d43097de100f5829a1/src/frontend/compiler.cpp#L2110-L2114) therefore do not support the preferred semantics as written. The already valid `(await job)?` form must remain valid. This is **source analysis**, not a claim from a new compiled test run.

### Proposed minimal grammar contract

For a syntactically unparenthesized `await` operand producing a task handle, trailing `?` should apply **to the awaited result**. In conceptual AST form:

```text
await job?       => propagate(await(job))
(await job)?     => propagate(await(job))
await (job?)     => await(propagate(job))    # explicit grouping remains meaningful
```

Do not achieve this by a blind post-parse rewrite of `await(propagate(...))`: the current AST shape alone can lose whether the user wrote explicit parentheses. Prefer one compiler-owned parsing/precedence adjustment that preserves grouping and normal postfix chains. Do not add an opcode, duplicate error lowerer or special-case runtime task-result layout.

Check chained calls (`await load()?`), simple handles, binary operators (`await job? + 1`), nested/parenthesized propagation, malformed source recovery, editor spans/completion and 1 MiB parser stacks. The desired check/lowering path must still perform **await → result inspection → `?` early return and cleanup**. The existing [`Op::await` and `Op::sum`/propagation lowering](https://github.com/Shmellyorc/Void.Language/blob/383d2ce0a404a4bbf3df01d43097de100f5829a1/src/frontend/compiler.cpp#L3659-L3669) provides the relevant mechanisms.

[ASYNC.md](https://github.com/Shmellyorc/Void.Language/blob/383d2ce0a404a4bbf3df01d43097de100f5829a1/docs/ASYNC.md) establishes native scheduled async calls, inferred task handles, repeated awaits, structured groups, cancellation distinction and checked cleanup. No new `job.await()`, `jobs.spawn`, detached tasks or unrestricted worker sharing is prerequisite. The Language Bible §42.1 and §63 are **illustrative older syntax**, with `jobs.spawn` in §63 still deferred; updating their examples must not misrepresent working features.

## C. Standard `error` and managed inheritance

- [INHERITANCE.md](https://github.com/Shmellyorc/Void.Language/blob/383d2ce0a404a4bbf3df01d43097de100f5829a1/docs/INHERITANCE.md#L15-L23) supports one **managed** base, normal ancestor views, original GC identity and no inline value inheritance. `struct error` (not `value struct error`) is therefore the appropriate base.
- With no custom `init`, ordinary named-field construction spans base and derived fields. Introducing a base `init` requires explicit `super.init(...)` in every derived initializer; avoid it unless Boss requires a mandatory message.
- Plain fields retain existing ownership, mutability, deep-readonly capabilities, GC roots and exact module visibility; no new error-specific field logic or implicit conversion is justified.
- A derived `network_error` remains its distinct nominal managed type. A regular upcast from an ordinary derived object to a base object already exists; **that is not permission to upcast entire `result` types or relax `?`**.
- A managed error inside `Err` requires existing result-payload GC tracing/retention to be validated in sync, async, repeated-await and JIT/AOT cases. This should reuse standard result/GC implementation, not duplicate it.

### Defaults are an actual language choice

[STRINGS.md L21–L26](https://github.com/Shmellyorc/Void.Language/blob/383d2ce0a404a4bbf3df01d43097de100f5829a1/docs/STRINGS.md#L21-L26) gives `string` the safe default `""`; [INITIALIZATION.md L66–L85](https://github.com/Shmellyorc/Void.Language/blob/383d2ce0a404a4bbf3df01d43097de100f5829a1/docs/INITIALIZATION.md#L66-L85) gives integers zero and uses safe defaults for omitted named fields. Consequently, in an **init-free** declaration with the approved three fields, `error(message = "oops")` is expected to get empty `path` and zero `code` **already**, without adding explicit `= ""` / `= 0` initializers. `error()` would likewise be default-constructible with an empty message unless constrained through a custom initializer or future rule.

**Recommendation:** keep the straightforward init-free three-field declaration as the first implementation; document empty-path and zero-code behavior as existing defaults. Explicit default initializer spellings are optional documentation clarity, not a new needed mechanism. Ask Boss **only** whether an empty message is acceptable or a message must be mandatory. Making it mandatory has a real construction/inheritance complexity cost; do not infer that requirement from the word “message.”

## D. Exact-error contract: potential source/IR mismatch to test

The existing [`propagate_expression` code](https://github.com/Shmellyorc/Void.Language/blob/383d2ce0a404a4bbf3df01d43097de100f5829a1/src/frontend/compiler.cpp#L2244-L2249) checks `Err` payload compatibility with `expect(...)`. The general [`expect`](https://github.com/Shmellyorc/Void.Language/blob/383d2ce0a404a4bbf3df01d43097de100f5829a1/src/frontend/compiler.cpp#L1923-L1929) explicitly accepts a managed `Derived` where `Base` is expected. But propagation [lowers to a new result sum](https://github.com/Shmellyorc/Void.Language/blob/383d2ce0a404a4bbf3df01d43097de100f5829a1/src/frontend/compiler.cpp#L3659-L3669) without an evident explicit payload upcast, while [IR sum validation](https://github.com/Shmellyorc/Void.Language/blob/383d2ce0a404a4bbf3df01d43097de100f5829a1/src/ir/module.cpp#L870-L876) relies on [a narrower `assignable` predicate](https://github.com/Shmellyorc/Void.Language/blob/383d2ce0a404a4bbf3df01d43097de100f5829a1/src/ir/module.cpp#L567-L569) that does **not** accept general derived-to-base payloads.

This supports a **specific potential discrepancy**, not an executed production defect. A minimal future regression is:

```void
struct base_error:
    message: string

struct child_error extends base_error:
    detail: i32

func inner() -> result<i32, child_error>:
    return Err(child_error(message = "oops", detail = 1))

func outer() -> result<i32, base_error>:
    var value = inner()?
    return Ok(value)
```

Under the Boss-approved exact policy, `outer()` should receive a **source-located compile-time error** for the mismatched `E`, not silently widen nor reach an internal IR failure. Also verify a positive case where `E` matches exactly and an independently explicit object upcast, so tightening result propagation does not remove ordinary inheritance.

**Important:** do not add implicit `result<T,Child>` → `result<T,Base>` conversions to solve this; never change the approved policy without returning it to Boss.

## E. Standard-library ownership, prelude and compatibility

`stdlib/error.void` is a natural **future module**: `use error` maps via the existing compiler-owned official standard resolver to the unique `/@stdlib/error.void` identity, distinct from project `libs/` and from quoted project paths. `error` is not a reserved token. Existing [module lookup](https://github.com/Shmellyorc/Void.Language/blob/383d2ce0a404a4bbf3df01d43097de100f5829a1/src/frontend/compiler.cpp#L1242-L1273) and [resolver](https://github.com/Shmellyorc/Void.Language/blob/383d2ce0a404a4bbf3df01d43097de100f5829a1/src/frontend/compiler.cpp#L4280-L4303) can provide explicit source imports without inventing a second standard-library mechanism.

**Recommendation:** ship the initial standard `error` as explicit `use error`, unless Boss specifically elects the extra automatic-prelude feature. An implicit prelude must define exactly one canonical type owner/identity, collision/shadowing/import precedence, reachability and compiler/editor/cache/reload identity; it is **not currently a free feature**. An explicit import still removes repeated *custom error declarations*.

Today's official [`stdlib/console.void`](https://github.com/Shmellyorc/Void.Language/blob/383d2ce0a404a4bbf3df01d43097de100f5829a1/stdlib/console.void) already defines `value struct io_error`; [`stdlib/time.void`](https://github.com/Shmellyorc/Void.Language/blob/383d2ce0a404a4bbf3df01d43097de100f5829a1/stdlib/time.void) commonly returns `result<...,string>`. Both remain valid. A new common managed `error` is **opt-in for APIs that choose it**, not an implicit breaking migration of existing value/string errors. Conversions, if wanted, should be explicit future library functions.

**Formatting:** Stage 10 gives managed structs without display hooks an opaque qualified-type display, not automatic `message` output. Its existing source-level `string() -> string` hook could, if Boss authorizes, return `self.message` from the `error` owner; no new formatter is necessary. Without it, `print("{err}")` does not by itself promise a human-readable error message. Formatting is a separate optional design decision; do not invent a compiler-reserved `error` rendering path.

## F. Asynchronous scene example: approved direction, unverified execution

```void
use error

# Illustrative; both loading functions, file I/O and import access require actual definitions.
async func load_scene() -> result<bool, error>:
    with tasks.group() as jobs:
        var world_job = load_world_data()
        var audio_job = load_audio_data()
        var world = await world_job?
        var audio = await audio_job?
        print("World: {world}")
        print("Audio: {audio}")
    return Ok(true)
```

These ordinary async calls already enqueue tasks. `jobs.spawn` is not needed. `await job?` is **proposed, not currently working source**; library loading functions and their `result<T,error>` signatures are assumed. A failed `Err` from an awaited result enters `?` cleanup/propagation; unlike a runtime panic or cancellation, it does not automatically cancel a sibling merely because the task returned `Err`. Scope early-exit policy still governs unawaited tasks.

## G. Required future acceptance, only once Boss authorizes implementation

1. **Precedence:** `await job?` equals `(await job)?` in AST, type, diagnostics and runtime; explicit `await (job?)` retains grouping; chained async calls, nested `?`, arithmetic, malformed editor recovery and 1 MiB parse/semantic stacks.
2. **Async lifecycle:** ordinary/optimized JIT and linked AOT; error result delivered through task frames; repeated awaits; early `?` cancellation/draining/cleanup and GC roots, without turning `Err` into cancellation or panic.
3. **Managed error:** `use error`, exact three types/field order, init-free defaults if approved, public inherited base/derived fields and visibility; legal named construction, object upcast and readonly/GC behavior; JIT/AOT, independent GC stress and editor/LSP/import/reload checks.
4. **Exact result error typing:** positive exact `E` and negative derived-`E` propagation, with source diagnostics rather than internal IR mismatch; `result<T,string>` and existing `io_error` APIs unaffected.
5. **Display (only if separately approved):** hook/default behavior for base/derived managed errors through the established Stage 10 formatter, without private-field leakage.
6. **Cross-platform stage tests:** focused Linux Debug/Release/strict sanitizers, JIT/O2/AOT and the project-mandated final native matrix at a future authorized stage lock, not after every doc or small syntax edit.

## H. Boss decisions to return, not assume

1. **Exposure:** Initial `use error` module (recommended, no prelude magic) **or** an automatic core prelude?
2. **Message construction:** Is empty `message` an acceptable safe default in an init-free managed error (recommended for simplicity), or must every construction supply it? The latter needs explicitly designed `init`/derived initializer chaining.
3. **Display:** Should standard `error` define an ordinary `string() -> string` method that displays `message` (optional), or retain the existing opaque default?

**Already decided, no reapproval requested:** the three fields/types, managed inheritance for specialized errors, exact `E` propagation, and the preferred `await job?` meaning. The static source/IR discrepancy in §D is a verification/repair concern, not authorization to relax that policy.

## I. Stage/governance boundary and provenance

This is a design/evidence document for future work. Stage 10's owner-approved scope remains the 44-step console/display/fields/operators mission and **is not yet LOCKED**. The native stack follow-up and genuine Windows console/ConPTY evidence decisions are separate. Stage 6's locked async scope remains intact. Research Clanker plans any later authorized language/std-library stage; Golden Wrench reviews architecture; Boss approves; Keyboard implements; Boss alone commits/pushes/dispatches CI and declares locks. Protected root `README.md` is untouched.

**Independent inspection:** published `main` at `383d2ce0a404a4bbf3df01d43097de100f5829a1`; `docs/LANGUAGE_BIBLE.md` §§29, 42.1, 63; `docs/ASYNC.md`; `docs/ADVANCED_LANGUAGE.md`; `docs/INHERITANCE.md`; `docs/INITIALIZATION.md`; `docs/MANAGED_STRUCTS.md`; `docs/STRINGS.md`; `docs/MODULE_SYSTEM.md`; `docs/stage10/DISPLAY_FORMATTING.md`; `docs/STAGE10_DESIGN.md`; `stdlib/console.void`, `stdlib/time.void`, `stdlib/README.md`; `src/frontend/compiler.cpp`; `src/ir/module.cpp`; `src/backend/llvm/backend.cpp`. No new compilation/run or new CI result is claimed.

**THREE CLANKERS. ONE VOID. STRONG BOLTS. NO GHOST BOLTS!** 🔧🤖🏮⌨️
