> **Boss amendment, 26 October 2026: IMPLEMENTATION AUTHORIZED.** All four
> decisions in RESEARCH section 8 are approved, with exact numeric names
> `rand_T()` / `rand_T(maximum)` / `rand_T(lower, upper)` and corresponding
> checked `try_rand_T` forms. The distinct public below/between recommendation
> below is superseded. A canonical-random-only arity adapter is authorized;
> general overloads are excluded. Historical research observations remain evidence
> of the baseline, not implementation claims. See IMPLEMENTATION.md for progress.

# Random version-1 contract

Boss approved implementation on 26 October 2026 from
`b374fd30aad70bd27c334150949b2a06481b1b67` with mandatory arity-based names.
[Research and independent vectors](RESEARCH.md); [mission](ROADMAP.md);
[implementation and verification](IMPLEMENTATION.md).

## 1. Approved API and canonical arity adapter

For every integer and float primitive T, the public spelling is `rand_T()` /
`rand_T(maximum)` / `rand_T(lower, upper)`. Integers use their entire domain
at zero arity; floats use [0,1). Bounded forms are exclusive. `rand_range` remains
inclusive and integer-only. Module `try_rand_T` has the same three arities;
generator checked methods accept one or two bounds. No public below/between
functions exist.

The compiler first resolves the ordinary public declaration and its canonical
`/@stdlib/random.void` identity. A small outlined adapter then selects an ordinary
private source implementation by supplied argument count. It reuses the existing
binder, exact numeric types, contextual literals, named argument mapping, source
expression evaluation order, receiver places and capability checks. Unsupported
counts diagnose before binding; duplicate labels and missing/unknown labels use
ordinary diagnostics. Aliases retain canonical identity. Project modules named
random receive no adaptation. Ordinary functions and methods retain one signature.

Module function references denote exactly their declared zero-argument signature.
Bounded forms are direct-call syntax; assigning a `func(T) -> T` reference to the
zero-arity public declaration rejects. Generator bound-method references retain
existing value-capture and receiver restrictions. A supported raw method value
advances its captured stream snapshot independently of the original generator;
a checked method value retains its declared one-bound signature. Compiler-owned completion lists all
supported arities; hover presents the selected call and signature help binds the
active bounded form with source parameter documentation. Navigation/references
point to the public source declaration; installed-source rename remains denied.

The required `generator(seed = ...)` initializer parameter is the sole canonical
exception to shadowing the module's `seed` function. It is checked by module,
receiver, initializer and exact parameter declaration location. This grants no
ordinary source shadowing or overload feature.

## 2. Exact approved public surface

`random` is the ordinary installed source module `stdlib/random.void`, imported
with `use random`. Public documentation uses the existing `##`, `@param` and
`@return` contract. Private state and compiler helpers remain inaccessible.

### Ownership and seeding

| Declaration | Contract |
|---|---|
| `const algorithm_version: i32 = 1` | Identifies the entire stream/sampling contract below |
| `value struct generator` | Four private mutable u64 state fields; no managed references or native resource |
| `func (self generator) init(seed: u64)` | Required explicit seed; nonfallible deterministic initialization |
| `func (self generator) seed(value: u64)` | Reset this generator to that seed's initial state |
| `func seed(value: u64)` | Reset the module default generator; mark it initialized without OS access |
| `func from_entropy() -> result<generator, error>` | New independent generator from one OS-supplied u64 seed |
| `func try_initialize() -> result<bool, error>` | Lazily seed the default if needed; Ok(true) whenever ready; no draw |

Examples use existing constructor and receiver grammar:

```void
use random

var world = random.generator(seed = 42)
var saved_position = world
var roll = world.rand_range(1, 6)
var same_roll = saved_position.rand_range(1, 6)

random.seed(42)
var index = random.rand_i64(items.count)
```

The last example assumes a nonempty existing `items` list. Its empty case is a
checked bounds failure, never an invented index. Copying `generator` duplicates
stream position; it does not share advancement. A mutating receiver requires a
writable place. Drawing through `const` or directly from a temporary rejects under
existing receiver rules. A custom required initializer prevents a public all-zero
default construction path. No destructor or manual close is required.

### Numeric families

For **each** suffix in `i8, u8, i16, u16, i32, u32, i64, u64`, let T denote
that exact primitive. The following table is an API expansion rule, not new VOID
syntax or a numeric constraint feature.

| Module function and generator method | Return | Semantics |
|---|---|---|
| `rand_T()` | T | Entire T domain, including signed minima and maxima |
| `rand_T(maximum: T)` | T | [0, maximum); require maximum > 0 |
| `rand_T(lower: T, upper: T)` | T | [lower, upper); require lower < upper |
| `try_rand_T(maximum: T)` | `result<T, error>` | Recoverable counterpart |
| `try_rand_T(lower: T, upper: T)` | `result<T, error>` | Recoverable counterpart |

For **each** T in `f32, f64`, expose the same five bounded/plain forms. `rand_T()`
means [0,1), rather than the full float domain. Bounded floats require finite
endpoints and strict positive width, as specified in section 6.

Also expose both module functions and generator methods:

```void
boolean() -> bool
rand_range<t>(lower: t, upper: t) -> t
try_rand_range<t>(lower: t, upper: t) -> result<t, error>
choose<t>(const items: [t]) -> t
try_choose<t>(const items: [t]) -> result<t, error>
shuffle<t>(items: [t])
shuffled<t>(const items: [t]) -> [t]
```

These lines are signature inventory; actual methods use `(self generator)`.
`rand_range<t>` accepts exactly the eight integer primitives, inferred using the
existing exact-type/literal rules. `rand_range(1,6)` is i32; use explicit type
arguments or typed operands for other widths. Mixed established numeric types,
bool, floats, enums and nominal numeric wrappers reject; no implicit coercions.
There is no inclusive floating-point API in this proposal.

Module-only checked counterparts `try_boolean() -> result<bool,error>` and
`try_rand_T() -> result<T,error>` cover first-use entropy failure. Module-only
`try_shuffle<t>(items: [t]) -> result<bool,error>` acknowledges success with
Ok(true); `try_shuffled<t>(const items: [t]) -> result<[t],error>` returns the copy.
Explicit generator raw draws and shuffles need no meaningless always-Ok duplicate:
they have no recoverable argument/entropy error. Existing allocation failures
still use checked Runtime failure.

Plain fallible convenience functions reuse checked operations through existing
`convert.unwrap` on scalar or other authority-compatible results. For collections,
unwrap scalar indices/readiness before returning element/copy values directly;
do not pass readonly success payloads through a mutable generic unwrap parameter.
Checked functions return module errors; plain functions raise an ordinary
source-located checked Runtime failure on those Errs.
No abort, C++ exception escape, zero-value fallback or hidden bound adjustment.
For an application wanting recovery everywhere, initialize using `try_initialize`
and use the applicable `try_` functions, or use `from_entropy` plus a generator.

### Recoverable error values

`random.error` is a managed struct extending `error.error`, with `kind:
error_kind` and `native_code: i32 = 0`. Inherited `message` explains the failure,
`path` is empty and `code` is the portable category number. Approved enum:

| Kind / code | Condition |
|---|---|
| `invalid_bounds` / 1 | Nonpositive exclusive upper bound, equal exclusive endpoints or reversed endpoints |
| `nonfinite_bounds` / 2 | Either float endpoint is NaN or infinity; checked before ordering |
| `empty_collection` / 3 | `choose` on no elements |
| `entropy_unavailable` / 4 | OS seed read failed; preserve errno or signed NTSTATUS bits in native_code |

Type errors remain compile-time diagnostics. Resource exhaustion follows existing
checked Runtime failure and is not mislabeled as a random argument error. An
invalid checked call consumes no random state and does not initialize the default.
An entropy error leaves the default unchanged/uninitialized; a later checked
attempt may retry. Once ready, `try_initialize` never silently reseeds.

## 3. Deterministic engine and version contract

Use `xoshiro256**` 1.0, initialized by
four consecutive SplitMix64 outputs in state order 0,1,2,3. Pin the published
[state transition](https://prng.di.unimi.it/xoshiro256starstar.c) and
[seed expander](https://prng.di.unimi.it/splitmix64.c), not a host standard-library
engine. Only their seed expansion and `next` operations are needed; jump,
arbitrary-jump and stream-management APIs are outside this scope.

Every explicit u64 seed, including zero and maximum, is used unchanged. The
SplitMix increment is 11400714819323198485, its multipliers are
13787848793156543929 and 10723151780598845931. All intermediate arithmetic is
u64 wrapping. Four consecutive expansion outputs cannot all be zero. Seed
expansion adds no entropy and does not promise statistically independent streams.

Version 1 freezes seed mapping, transition order, state copies, width extraction,
Boolean bit, rejection rules, float precision/formulas, traversal and draw
consumption. Equal explicit seeds and equal ordered calls/arguments yield equal
results on supported platforms in normal/O2 JIT and linked AOT, under VOID's
documented floating environment. OS-default seeds and interleaving between
different tasks do not carry a replay guarantee. Record a seed and explicit call
order for replay; do not rely on incidental scheduler ordering.

This algorithm version must not change silently in a future library release.
A different algorithm or sampler needs a separately named/versioned contract and
owner approval; version 1's sequences stay stable. Ordinary in-process generator
copies are snapshots. No portable serialized object-memory representation is
promised. The algorithm is gameplay RNG, **never cryptographic randomness**.
A future `crypto` module remains separate even though OS seeding uses a secure
system facility.

## 4. Small compiler and native boundary

Source already has wrapping u64 arithmetic and checked casts, but no XOR/shift/
rotate syntax. Keep engine state, initialization and transition in VOID with:

1. One private `_random_xor(a: u64, b: u64) -> u64` compiler operation, restricted
   to canonical `/@stdlib/random.void`. Add one independently validated u64 XOR
   IR operation and lower to LLVM XOR. It is runtime-only, pure and worker-safe.
2. Private `_random_domain_max<t>() -> u64`, accepted only for the eight integer
   types. Specialization returns an existing constant instruction with 2^N-1.
   This is a tiny type gate and constant, not another runtime operation or public
   reflection system. Reuse the existing numeric type registry.
3. Private `_random_seed() -> result<u64,i32>`, backed by the checked OS bridge
   in section 8. It is effectful, requires coordinated Runtime entry and is
   forbidden in worker-reachable code. The narrowly scoped arity adapter above preserves the approved public names.

These helpers are **new work**, modeled on the canonical binary-codec gate and
existing numeric bitcast precedent. They do not work at the research baseline.
Keep their calls in runtime source bodies; specializing domain metadata into a
constant instruction does not authorize new const/default expression calls.
No user-visible bitwise operators, permissive generic native call, native RNG
object, state pointer, extra GC root or whole-state C ABI is necessary.

Fixed shifts use unsigned division or wrapping multiplication by powers of two.
For u64 x, rotate-left-7 is `x * 128 + x / 144115188075855872`; rotate-left-45 is
`x * 35184372088832 + x / 524288`, all wrapping. The two pieces occupy disjoint
bits. SplitMix's logical right shifts are division by 2^30, 2^27 and 2^31;
xoshiro's left shift by 17 is wrapping multiplication by 131072. This keeps the
authoritative algorithm in source without per-bit loops or codec allocations.
Use decimal source constants; VOID does not currently accept hexadecimal literals.

## 5. Integer sampling, including the entire 64-bit domain

Let D = 2^N-1 from the integer-domain helper. Each candidate uses the high N bits
of one new u64 output. Source can extract them without shifts: when D is u64.max,
use the word; otherwise divide it by `u64.max_value / (D + 1) + 1`.
Do not cache leftover low bits; each attempted candidate consumes one word.

Map signed T into monotone unsigned ordinals with offset `D / 2 + 1`:

- Negative x: `(x - T.min_value) as u64`; the signed subtraction fits.
- Nonnegative x: `(x as u64) + offset`.
- Inverse ordinal below offset: `T.min_value + (ordinal as T)`.
- Otherwise: `(ordinal - offset) as T`.

Unsigned ordinals are ordinary checked mathematical conversions. Validate bounds
in T first. Subtract ordinals in u64; never subtract signed extrema or cast a
negative integer directly to unsigned.

Exclusive width is `upper_ordinal - lower_ordinal`. Inclusive width is that
difference plus one in wrapping u64. Width zero is legal **only** for the already
validated complete 64-bit inclusive domain; use one raw word in that case.
It never makes `rand_T(0)` valid. Narrow complete domains have representable
widths, e.g. 256 for u8.

For positive width w, reject candidates below threshold, then return candidate
modulo w. Threshold is `(D + 1) % w` when N < 64 and `(-w) % w` in wrapping u64
when N = 64. Handle w = 1 first: return the sole value with no draw. Handle the
full64 zero-width sentinel before any remainder. The accepted candidate count is
a multiple of w, removing mapping-induced modulo bias. This generalizes the
[published bounded-rejection method](https://www.pcg-random.org/using-pcg-c-basic.html).
It does not imply that successive PRNG words are physically independent.

`rand_T()` returns the inverse ordinal of its high-N candidate, so signed full
draws include negatives. Inclusive full-domain draws agree with `rand_T()` for
the same starting state. `rand_range(x,x)` returns x without a draw. All other
valid non-singleton integer ranges use rejection; no float-based integer scaling,
absolute-value trick, `upper+1` in T, retry cap or biased fallback.
`boolean()` consumes one word and returns whether its most significant bit is 1.

## 6. Floating-point sampling

`rand_f32` uses the high 24 bits, converted exactly, divided by 16777216 in f32.
`rand_f64` uses the high 53 bits divided by 9007199254740992 in f64. Equivalently,
first divide the raw u64 by 1099511627776 or 2048. Both generate equally spaced
dyadic values in [0,1), include positive zero and exclude one. Never convert the
whole u64 word to float first. The [author's floating conversion explanation](https://prng.di.unimi.it/#generating-uniform-doubles-in-the-unit-interval)
supports the binary64 construction; binary32 follows the same precision argument.

For `rand_T(lower,upper)`, validate both endpoints finite before testing
`lower < upper`. Equal endpoints, including -0/+0, are an invalid empty exclusive
interval. `rand_T(maximum)` uses positive zero as lower and requires finite
maximum > 0. No automatic swap or absolute value. Finite checks are source-
expressible using self-equality and T's finite min/max constants.

For each attempt, draw the unit value u in that same T and evaluate, with separate
nearest/ties-even operations at T precision:

- If lower < 0 and upper > 0: `lower * (1 - u) + upper * u`.
- Otherwise: `lower + (upper - lower) * u`.

Accept only if `value >= lower and value < upper`; otherwise draw again. The
weighted case avoids overflowing an opposite-sign difference. Rejection handles
rounding to the excluded endpoint without epsilon or a new predecessor primitive.
u = 0 supplies an accepted lower endpoint for every valid interval. No fixed
retry limit or biased fallback is added. Retry count is part of stream behavior.

This is a conditional scaled-grid distribution approximating uniform real
interpolation, **not equal probability for every representable float**. Signed
zero from bounded arithmetic follows existing IEEE operations and may normalize;
it does not promise preservation of the lower endpoint's zero sign. Unit draws
always return positive zero when their extracted bits are zero. No NaN/infinite
result is published from a valid bounded call. No fast-math, reassociation, FMA,
flush-to-zero mode change or host `uniform_real_distribution` is introduced.

## 7. Collections and state advancement

`choose` selects a position with an unbiased exclusive i64 index. Duplicate values
occupy separate positions. Empty input returns empty_collection through try_choose;
one element returns its ordinary value without drawing or initializing entropy.
Because the input parameter is const, selected managed/list/dictionary elements
and references inside value/fixed children are readonly views, even when the
original caller list was mutable. Identity is retained; reference-free values
remain independent writable copies. To select a writable managed element, draw
an index and access the caller's separately held mutable list directly. No deep
clone or capability restoration is implied.
Elements need no equality/hash/display support; NaN elements are valid data.

`shuffle` performs descending Fisher–Yates: for i = count-1 down through 1,
draw j uniformly in [0,i] and exchange i and j. An index draw still occurs when
j equals i, but the self-swap may be skipped. Reuse the same i64 sampler; do not
use floating scaling. Empty/singleton lists consume no words or entropy.
Count, identity and structural version stay unchanged; aliases see replacements.
Its ordinary mutable `[t]` parameter also requires an authority-compatible element
profile. A mutable outer slice with readonly referenced children cannot simply
be forwarded to that public parameter; this is existing collection invariance.
Complex element snapshots/GC barriers use existing array lowering. It is not a
structural mutation exemption or permission for unsafe concurrent access.

`generator.shuffled` makes an ordinary complete exclusive slice, then performs
the short Fisher–Yates traversal on that local list, sharing the same index
sampler. Do not forward it through `shuffle<t>(items:[t])`: that mutable generic
boundary can reject its readonly child profile. The original outer list is
unchanged, including on a later failure. Referenced children remain shared and
readonly because the input formal is const, even for mutable caller input.
This does not grant writable access to a readonly child. Allocation failure during
in-place complex swaps may leave earlier completed swaps/state advancement; do not
claim transactional rollback or universal allocation-free operation. Module
shuffled/try_shuffled first ensure readiness when count > 1, then directly return
the generator copy/result without generic unwrap of readonly list payloads.
Seed failure leaves input/default state unchanged. A subsequent copy-allocation
failure may leave the default seeded, with no word drawn; later swap failures can
occur after draws. Empty/singleton copies skip entropy.

Likewise choose/try_choose share a checked scalar index helper: plain choose
unwraps the index and returns `items[index]`; checked choose propagates the index
error and constructs Ok from that element directly. This preserves readonly
facts without a new generic capability feature or an unwrap implementation.

These generic signatures accept growable `[t]` only. Fixed arrays do not match
that type today and ordinary fixed parameters copy. For a fixed array use
`fixed[0..<fixed.count]` to obtain an independent growable list, including empty
fixed arrays. In-place fixed-array mutation, new views and const generics are not
silently added. This boundary requires explicit acceptance in the owner decisions.

## 8. Default lifetime, entropy and platforms

Keep a valid private source `generator(seed = 0)` global and a private readiness
Boolean initially false. Ordinary runtime globals already provide one shared
default per application Runtime, precise roots, owner checks, startup failure
handling and compatible-reload retention. Do not add a C++ Runtime RNG singleton.
The initial fixed state is never used for a default draw before readiness.

Type/argument validation and no-draw singleton handling precede entropy. The first
operation needing a word obtains a seed, constructs the complete generator and
publishes it, then marks ready. Calls are synchronous and introduce no await or
user callback between those steps. Operate on the actual global receiver, or
explicitly commit a modified value copy; discarding a local copy loses advancement.

Import resolution, checking, completion, cache population and importing random
without invoking a draw never request entropy. Explicit seeded generators never
request entropy either. An application initializer that deliberately calls a
default draw can request entropy during normal eager startup. Compatible reload
retains default cells and explicit
generator values; new application Runtimes, including cache hits, begin fresh.
Runtime close follows existing provider/root teardown. Compatible reload does not
replay initializers or reseed. Algorithm/constants/layout/initializer changes use
existing restart contracts; do not install a second state-migration mechanism.

Acquire exactly eight OS bytes into native stack storage, verify full success,
decode little-endian into u64, then use the same deterministic seed expansion.
Zero is a valid OS seed. Do not publish partial bytes or use time/PID/hash/address/
`std::random_device` fallback. An OS seed is probabilistic, not collision-free.

| Host | Required native call | Integration |
|---|---|---|
| Linux | `getrandom(buffer,8,0)` | Retry EINTR/complete short reads; fail on zero progress/real error; ENOSYS is a checked failure. May block during early initialization. [Manual](https://man7.org/linux/man-pages/man2/getrandom.2.html) |
| Windows | `BCryptGenRandom(nullptr,buffer,8,BCRYPT_USE_SYSTEM_PREFERRED_RNG)` | Check NTSTATUS; use Bcrypt.lib for Runtime targets and installed AOT flags. [Microsoft](https://learn.microsoft.com/en-us/windows/win32/api/bcrypt/nf-bcrypt-bcryptgenrandom) |
| macOS | `getentropy(buffer,8)` | Check return/errno; system library, macOS 10.12+. No Security framework. [Apple](https://raw.githubusercontent.com/apple-oss-distributions/xnu/main/bsd/man/man2/getentropy.2) |

Use existing NativeSafe coordination around the OS call, with no managed pointers
in its buffer; return to Managed before constructing errors. A narrow noexcept
bridge can return successful bridge execution separately from an OS status, with
u64 seed and i32 native-code output slots. Its private source result is
`result<u64,i32>`; zero native code means OS success, including seed zero.
Unexpected bridge/coordination failure uses the existing RuntimeFailure channel.
No exception crosses JIT code. JIT symbol binding and AOT archive/link dependencies
must both be wired; the new source module already fits recursive installation.

## 9. Workers, async and replay boundaries

Default access is owner-only under existing global effects. Neither default APIs
nor OS seeding is allowed in worker-reachable code. Do not add a lock or broaden
the worker memory model. A directly named worker helper can receive a u64 seed
or scalar generator snapshot, bind a local mutable copy and call deterministic
methods. The owner's original copy does not advance. Returning scalar generator
state remains subject to ordinary worker-transfer validation.

Direct `spawn generator.method()` is not supported by current spawn grammar.
Managed error/list values cannot be transferred as worker arguments/results;
normal local operations remain subject to current graph/effect checks. Do not
promise that every generic specialization is worker-safe. Async owner tasks may
interleave calls to the same default; use separate explicitly seeded generators
for stable per-system streams. No source async value-receiver methods, automatic
stream splitting, scheduler changes or synchronization primitives are needed.
