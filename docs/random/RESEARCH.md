> **Boss amendment, 26 October 2026: IMPLEMENTATION AUTHORIZED.** All four
> decisions in RESEARCH section 8 are approved, with exact numeric names
> `rand_T()` / `rand_T(maximum)` / `rand_T(lower, upper)` and corresponding
> checked `try_rand_T` forms. The distinct public below/between recommendation
> below is superseded. A canonical-random-only arity adapter is authorized;
> general overloads are excluded. Historical research observations remain evidence
> of the baseline, not implementation claims. See IMPLEMENTATION.md for progress.

# Random: verified foundation and design decisions

26 October 2026. Audited baseline **`b374fd30aad70bd27c334150949b2a06481b1b67`**.
**Research only.** This report reconciles independent numeric, Runtime and
algorithm reviews. [Proposed API](DESIGN.md); [implementation roadmap](ROADMAP.md).
No production implementation, repository build, hosted CI or Git mutation was performed.
Standalone mathematical/reference experiments are distinguished below from VOID
execution. Six existing-compiler frontend checks were added during final design
review; no generated code or implemented random module was executed.

## 1. Established direction versus proposals

Boss established an ordinary `random` stdlib module, gameplay rather than secure
randomness, explicit exclusive/inclusive integer behavior, typed draws and
investigation of seeded generators/choose/shuffle. The
[24 October handoff, section 6](../VOID_STDLIB_AND_GLOBAL_VARS_WRENCH_HANDOFF_2026-10-24.md#6-random-deliberately-distinct-exclusive-and-inclusive-apis)
leaves full default ranges and several numerical policies unresolved. Its sample
overload spellings are design direction, not implemented call grammar.

The following **recommendations are not silently owner-approved semantics**:
distinct bounded names, full signed no-argument ranges, xoshiro256**/SplitMix64,
copyable generator values, checked/plain pairs, strict empty exclusive intervals,
integer-only inclusive ranges, bounded float rejection and growable-list scope.
Boss decisions at the end cover these before engineering.

Earlier Bible and dated handoff exclusions sometimes predate implemented features.
This audit used the real parser, numeric kernel, semantic/IR validation, lowering,
Runtime, library source and current tests. Stage 11 globals and the documentation
repair are present; that does not make this future module implemented or accepted.

## 2. Feature-completeness matrix at this baseline

| Area | Already implemented / reusable | Missing for this mission |
|---|---|---|
| Module resolution | Canonical stdlib source imports, nested paths, aliases, protected installed sources | `stdlib/random.void` |
| Numbers | Eight wrapping integer primitives, f32/f64, exact limits, checked casts | Random distributions and integer-domain specialization hook |
| Calls | Single ordinary declaration, named/default arguments, generic specialization | No function overload sets or numeric optional-argument representation |
| State | Private mutable fields, value copies, mutating receiver storage, runtime globals | Public generator type and default source state |
| Core | Arithmetic primitives and private IR bitcast precedent | One private u64 XOR operation; source PRNG |
| Entropy | NativeSafe and checked bridge patterns | OS seed acquisition, JIT binding, Windows bcrypt link integration |
| Errors | Managed base error, result, checked propagation, convert.unwrap | Module-specific error kinds and validation |
| Collections | Generic growable indexing, replacement, fresh full slices | choose, Fisher–Yates, copy-returning shuffle |
| Lifecycle | Runtime-owned global roots, admission, reload, cache separation | Random-specific lifecycle regressions |
| Workers | Scalar value transfer, transitive global/native restrictions | Explicit entropy effect rejection and pure-XOR acceptance |
| Tooling/install | Compiler-owned docs/completion/signatures/navigation, recursive module install | Real random declarations/docs and focused regression coverage |

## 3. Source evidence

Line numbers refer to the audited commit, not historical documents' old source
positions. Links open current local files; recheck if engineering rebases.

| Finding | Actual source / contract |
|---|---|
| Integer add/subtract/multiply/negation wrap; conversions instead check mathematical range | [numeric.cpp](../../src/numeric.cpp), lines 33–43, 80–107, 131–146; [LLVM backend](../../src/backend/llvm/backend.cpp), 1431–1448, 1486–1499; [numeric contract](../NUMERIC_TYPES.md), 64–103 |
| Lexer/parser have arithmetic/Boolean comparisons, no source XOR/shift/rotate; literals are decimal | [compiler.cpp](../../src/frontend/compiler.cpp), 251–267, 866–873; [IR operations](../../include/void/ir.hpp), 127–131 |
| Duplicate ordinary function names diagnose, regardless of signature | [compiler.cpp](../../src/frontend/compiler.cpp), 5070–5080 |
| `i32?`, nullable arrays and nullable generic parameters are unsupported | [compiler.cpp](../../src/frontend/compiler.cpp), 795–834; do not suggest nullable numeric defaults |
| Defaults are compile-time values with no omitted-argument runtime token | [call contract](../STAGE8_CALLS_NUMERIC.md), 39–79; existing shared argument mapper |
| Canonical private representation calls already gate on stdlib identity | [compiler.cpp](../../src/frontend/compiler.cpp), 3017–3020; [binary_codec.inc](../../src/frontend/binary_codec.inc), 1–42 |
| Ordinary codec uses u64 division/remainder, not public bit casts | [binary.void](../../stdlib/encoding/binary.void), 32–74; byte arrays would be excessive for every random step |
| Generic readonly list element return is real, including result payloads; const input narrows returned referents even from mutable caller input | [sprint3.py](../../tests/stage8/sprint3.py), 62–63 and 195–199; [capabilities.inc](../../src/frontend/capabilities.inc), 476, 523; [generics](../GENERICS.md), 29–35, 45–83 |
| Array inference requires matching fixed/growable kinds; no generic extent/view | [generic_inference.inc](../../src/frontend/generic_inference.inc), 47–51; [slicing contract](../ARRAY_OPERATIONS.md), 56–83 |
| Mutating value methods use the original selected place, including global receivers | [capabilities.inc](../../src/frontend/capabilities.inc), 124 onward; [receiver contract](../RECEIVER_METHODS.md), 68 onward |
| Required init cannot be bypassed by an all-zero default value | [initialization contract](../INITIALIZATION.md), 95–118 |
| Global cells use persistent Runtime Handles and guarded publication/access | [runtime ABI](../../src/runtime/abi.cpp), 15–64; [globals contract](../RUNTIME_GLOBALS.md), 96–136 |
| New JIT/cache-hit programs have fresh Runtime state; replacement reuses it | [LLVM backend](../../src/backend/llvm/backend.cpp), 1799 onward, 1902 onward, 1935 onward |
| Close drains providers while global roots still exist | [coordination.cpp](../../src/runtime/coordination.cpp), 349 onward |
| Value-only worker transfer and transitive global/clock/native restrictions are enforced independently | [IR validation](../../src/ir/module.cpp), 173–182, 1363–1388; [spawn checking](../../src/frontend/compiler.cpp), 2232 onward |
| Existing unwrap converts result errors into checked Runtime failure | [convert.void](../../stdlib/convert.void), 51–55; [stdlib_conversion.inc](../../src/frontend/stdlib_conversion.inc), 29–37 |
| NativeSafe permits native stack work before managed publication | [temporal_abi.cpp](../../src/runtime/temporal_abi.cpp), 10–49; reuse the pattern, not the temporal operation registry |
| JIT native symbols are explicitly bound | [LLVM backend](../../src/backend/llvm/backend.cpp), 1752–1755 |
| Runtime links and native installed flags are separate obligations | [CMakeLists.txt](../../CMakeLists.txt), 89–92, 189–205; [NativeBuild.cmake](../../cmake/NativeBuild.cmake), 7–9, 43–49 |
| Import completion enumerates actual stdlib files through VFS | [compiler.cpp](../../src/frontend/compiler.cpp), 6231 onward; [tooling.cpp](../../src/frontend/tooling.cpp), 8 onward |
| Existing abnormal-exit preservation keeps exact source, hashes and provenance | [crash.py](../../tests/text_encoding/crash.py), 21–72; [io execution harness](../../tests/io/execution.py), 21 onward; [preservation regression](../../tests/io/preservation.py), 16 onward |

No existing entropy implementation was found in production source, headers or
CMake. Do not treat native-build temporary-name randomness, hashes, timestamps or
the Python tests' random module as an application entropy provider.

## 4. Why ordinary defaults do not solve the illustrated API

`rand_i32(lower=0,upper=max)` treats one positional argument as lower, violating
Boss's example. Reversing the parameters breaks the two-argument example. Reserving
min/max/zero as an omitted sentinel excludes legitimate full-width arguments.
Nullable numeric parameters do not compile. Boxing a bound would change the
public argument types and require explicit wrappers; there is no automatic union
or boxing conversion to assume. A new variadic argument feature would exceed scope.

Distinct `rand_i32`, `below_i32` and `between_i32` use today's ordinary syntax,
give named arguments stable meanings and need no editor exceptions. Forwarding
typed entry points reuse generic integer sampling; they are not eight engines.

An alternative preserving literal 0/1/2-argument forms would need an explicitly
approved, canonical-module arity adapter before ordinary binding. It must account
for supplied argument order, named labels, generics, mutable receiver places,
function values, error locations, signature-help alternatives, references and
rename. The public name cannot pretend to be three ordinary declarations.
This is smaller than general overload resolution but still new compiler behavior.
It is unnecessary for the recommended named API, and should not be added merely
to conceal this decision from Boss.

For comparison, [.NET Random.Next](https://learn.microsoft.com/en-us/dotnet/api/system.random.next?view=net-10.0)
uses actual overloads. [Python random](https://docs.python.org/3/library/random.html)
provides exclusive/inclusive selection and in-place shuffle; its keyword warning
for randrange demonstrates why reinterpreting positional arities needs care.
These precedents inform ergonomics, not an instruction to copy their default
signed ranges, equal-bound policy, unlimited integers or thread model.

## 5. Algorithm and boundary tradeoffs

| Candidate | Assessment |
|---|---|
| Host `rand`/standard distributions | Do not use: their implementation/version behavior is not VOID's pinned stream contract |
| PCG32 | Compact documented 16-byte generator, direct 32-bit output; full u64 needs an additional specified construction. The author's two-generator example uses two states. Viable, but less direct for this complete 64-bit API. [Primary documentation](https://www.pcg-random.org/using-pcg-c-basic.html) |
| xoshiro256** 1.0 + SplitMix64 | Recommended: four u64 words, direct u64 output, explicit published transition/seed expansion; no 128-bit source math. [Transition](https://prng.di.unimi.it/xoshiro256starstar.c), [seed expansion](https://prng.di.unimi.it/splitmix64.c) |
| SplitMix64 as the public engine | Prefer its intended seeding role here; its full-period permutation has distinctive collision behavior. See the author's explanation in the linked source |
| Whole native generator object | Unnecessary resource/ABI/lifetime work; source value fields already fit |
| Whole private native transition | Feasible fallback design, but larger aggregate ABI/lowering surface than one private scalar XOR |
| Private XOR + ordinary source arithmetic | Recommended after independent review: exact constant-count shifts/rotations from existing arithmetic; no per-bit loop, managed allocation or state ABI |

The private domain-max specialization is needed for clean integer-only generic
diagnostics. Its mathematical value is source-computable, but forcing an unused
`% 1` operation merely to reject float specializations would obscure intent.
Return an ordinary constant using existing numeric metadata; do not build a
reflection or validation framework. XOR is runtime-only, so its introduction
does not require making arbitrary runtime calls legal in defaults/constants.

### Collection capability boundary confirmed during review

Const list input projects child references readonly. A new slice restores mutable
outer storage, but not child authority. Generic inference strips readonly from
inferred type parameters ([generic_inference.inc](../../src/frontend/generic_inference.inc),
37–49), while ordinary parameters must honor their declared capability
([capabilities.inc](../../src/frontend/capabilities.inc), 85–89). Mutable lists
have invariant element authority ([module.cpp](../../src/ir/module.cpp), 337).
Consequently `shuffle(readonly_child_slice)` and
`convert.unwrap(result_with_readonly_managed_success)` can reject legitimately.

The design uses a shared scalar index for choose, and direct const-input
generator.shuffled with local swaps. Module wrappers unwrap/propagate only scalar
readiness or indices, returning the capability-bearing values directly. This
source-only solution was checked with the existing frontend; it avoids modifying
convert, weakening readonly or adding a view/capability-polymorphism feature.
It intentionally returns readonly referenced choices/copy children. Decision 4
must make that user-facing consequence explicit.

The sources above grant broad reuse/public-domain permission. Engineering should
record the exact adopted core reference and attribution; current upstream files
also contain unrelated jump facilities. Do not import an evolving whole library
or automatically change VOID's versioned transition when upstream changes.

### Numerical recommendation

Use one N-bit rejection sampler expressed with u64 intermediates and high-bit
candidates, including explicit full64 handling. Signed monotone ordinals avoid
VOID's checked signed/unsigned conversion traps. This handles all inclusive
extrema without `max+1` in the source type or a new wide integer/multiply-high API.
DESIGN.md defines the precise width, singleton and raw-draw equivalences.

Unit floats need only existing exact integer conversion and division. For bounded
floats, naive scaling can overflow the difference or round to the excluded upper
endpoint. A predecessor/clamp helper would work, but source-level rejection after
a sign-aware interpolation avoids that extra primitive. No universally short
retry bound has been proved; do not advertise one. No fixed retry cap or altered
endpoint is recommended. The declared distribution is a scaled floating grid,
not uniform sampling of float bit patterns.

## 6. Native entropy and architecture

The recommended seed calls and integration points are in DESIGN.md section 8.
[Linux getrandom](https://man7.org/linux/man-pages/man2/getrandom.2.html) is fd-free,
supports small seed requests and may block before initialization; check the
return count and errors. Its documented API baseline is Linux 3.17/glibc 2.25.
[BCryptGenRandom](https://learn.microsoft.com/en-us/windows/win32/api/bcrypt/nf-bcrypt-bcryptgenrandom)
supports the null-provider system-preferred call and requires Bcrypt.lib.
[Apple getentropy](https://raw.githubusercontent.com/apple-oss-distributions/xnu/main/bsd/man/man2/getentropy.2)
supports this eight-byte request on macOS 10.12+ without Security.framework.
Those API requirements fit the repository's current hosted platform families;
they do not establish acceptance on older deployment targets or native execution.

Seed acquisition is the only new native OS facility. Source owns generator state,
seeding expansion, errors, bounds and collections. The native bridge uses stack
scalar output, separates OS failure from Runtime failure, and re-enters Managed
before managed error allocation. Add the explicit entropy worker effect; do not
grant workers arbitrary native-call permission. Pure XOR needs no C++ callback,
Runtime provider, code pointer, scheduler job or resource registration.

Default state belongs in existing source globals. A process-global C++ singleton
would incorrectly merge independent Runtimes and complicate reload/GC. A managed
generator would also work but would share identity on assignment; replay-friendly
value copies are simpler. No mutex is needed for the owner-only default. Worker
helpers receive value snapshots or seeds; immutable strings/scalars and existing
transfer rules are unchanged. Separate seeds are not a claim of independent or
nonoverlapping streams.

Windows requires bcrypt both on `void_runtime` consumers and in generated
`void-native.txt` flags for user-built executables. Preserve current staging,
atomic publication, Unicode/path and LLVM 19.1.7 contracts. No native-build
discovery rewrite is involved. Runtime archive hashes already participate in
cache identity; never serialize live RNG cells into cached code.

## 7. Research checks performed and their limits

The numerical checks below are independent models/reference experiments, **not
execution of an implemented VOID random module**. Temporary reference C files/binaries used by the
algorithm review were removed; this report records their observations, not a
retained instrumented compiler or reproducible acceptance artifact bundle.

- Python modulo-2^64 model and separately compiled C reference transcription
  agreed on all four initial state words and first eight outputs for seeds 0,
  42 and u64.max. The root audit independently recomputed those vectors.
- Exhaustive eight-bit accepted-candidate enumeration verified equal residue
  counts for all 256 positive widths, including full width. It checks mapping
  arithmetic, not PRNG independence or the generated LLVM instruction stream.
- Root audit checked 28 signed ordinal round trips across all signed widths and
  eight fixed-rotation equivalences at representative raw words.
- The independent float review enumerated all 2^24 f32 unit inputs for 20 boundary
  interval pairs, using C O2 without fast-math/FMA contraction. Adjacent positive,
  negative, maximum-finite, subnormal/normal-boundary pairs accepted about half;
  full opposite-sign finite extremes accepted all; ordinary [0.5,1) rejected one
  grid value. No candidate below lower or nonfinite result was observed in those
  cases. This is practical case evidence, not a universal retry-rate proof,
  exhaustive f64 test or VOID JIT/AOT result.
- Six frontend-only source probes ran using the existing Release voidc. All six
  produced their expected outcomes: two positive checks and four deliberate
  readonly diagnostics. No runner, JIT/AOT invocation, debugger, local CTest
  suite or hosted job was run. Native Windows/macOS branches and actual XOR
  lowering remain untested proposals.

The frontend probes used `voidc check <scratch/main.void>`, existing crash
preservation and the current installed-style convert/error imports. They confirmed
rejection of readonly payload unwrap and mutable forwarding, acceptance of local
copy swaps and the corrected scalar-index/readiness design, and rejection of
mutation through both returned managed references. Scratch inputs remain at
`/tmp/void-random-source-audit-94dbojo2/`; this is a local observation, not a
permanent acceptance bundle. Compiler SHA-256:
`49037597669211d753bc667e8ced1a329dddf37b7e62adac94db747e205d69c5`.
It was not rebuilt for this assignment; source inspection remains authoritative
for the audited commit. The corrected all-features capability probe's SHA-256 is
`bf33edffb5d7ececf7bf49e8b78df6f8f724386290c44b793ea59e3636b7701d`.
All four negative results were ordinary VOID2007 diagnostics, not crashes.

### Candidate reference vectors

Hexadecimal here describes bits, **not valid current VOID literal syntax**.
Initial state words after seed expansion:

```text
seed 0:
e220a8397b1dcdaf 6e789e6aa1b965f4 06c45d188009454f f88bb8a8724c81ec
seed 42:
bdd732262feb6e95 28efe333b266f103 47526757130f9f52 581ce1ff0e4ae394
seed 18446744073709551615:
e4d971771b652c20 e99ff867dbf682c9 382ff84cb27281e9 6d1db36ccba982d2
```

First eight raw outputs (`rand_u64`) for each initial state:

```text
seed 0:
99ec5f36cb75f2b4 bf6e1f784956452a 1a5f849d4933e6e0 6aa594f1262d2d2c
bba5ad4a1f842e59 ffef8375d9ebcaca 6c160deed2f54c98 8920ad648fc30a3f
seed 42:
15780b2e0c2ec716 6104d9866d113a7e ae17533239e499a1 ecb8ad4703b360a1
fde6dc7fe2ec5e64 c50da53101795238 b82154855a65ddb2 d99a2743ebe60087
seed 18446744073709551615:
8f5520d52a7ead08 c476a018caa1802d 81de31c0d260469e bf658d7e065f3c2f
913593fda1bca32a bb535e93941ba525 5ecda415c3c6dfde c487398fc9de9ae2
```

Independent candidate mixed-call oracle, fresh seed 42, in this exact order:

| Call | Expected value / representation |
|---|---|
| `boolean()` | false |
| `rand_i32()` | -519775866 |
| `below_i32(10)` | 0 |
| `between_i32(-5,10)` | -1 |
| `rand_range<u64>(0,u64.max_value)` | bits fde6dc7fe2ec5e64 |
| `rand_f32()` | IEEE bits 3f450da5 |
| `rand_f64()` | IEEE bits 3fe7042a90ab4cbb |
| `choose([10,20,30])` | 10 |
| `shuffle([0,1,2,3])` | list becomes [0,1,3,2] |

This sequence consumes 11 raw words. Additional independently derived natural
rejection candidates avoid a production-pluggable RNG test interface:

| Fresh seed / call | Expected candidates and result |
|---|---|
| 42, `below_u8(129)` | High-byte candidates 21,97,174; threshold 127; reject first two; return 45 after three words |
| 0, f32 between 1 and next representable above 1 | First two scaled results equal excluded upper; third is 1; three words |
| 0, f64 between 1 and next representable above 1 | Same three-word rejection pattern |

The float upper endpoint bits are 3f800001 and 3ff0000000000001, respectively;
the lower bits are 3f800000 and 3ff0000000000000. These were checked in an explicit
rounding reference model. Engineering must reproduce them through actual VOID,
then compare the following raw word to confirm consumption. Do not create all
expected results by calling the implementation itself.

## 8. Owner decisions: approved 26 October 2026

1. **Bounded API spelling:** Boss overrides the historical named recommendation.
   `rand_T()` / `rand_T(maximum)` / `rand_T(lower, upper)` are mandatory, with
   matching module checked forms and one/two-bound generator checked methods.
   A canonical-random-only compiler arity adapter is explicitly authorized.
2. **Failure/endpoints:** plain convenience and recoverable checked variants;
   strict empty exclusive bounds; inclusive singleton no-draw; empty choose error;
   finite-only bounded floats and ordered source rejection are approved.
3. **Replay/ownership:** version-1 xoshiro256**/SplitMix64, exact u64 seeds,
   full signed raw domains, value snapshots and per-Runtime lazy defaults approved.
4. **Collections:** growable choose/shuffle/shuffled, fixed-array slicing boundary
   and readonly returned child references are approved. Mutable selection uses an
   index and the caller's separately held mutable list.

Earlier recommendations and baseline findings in this research are historical.
The API spellings in the retained candidate tables mean the corresponding
approved `rand_T` call arity. Current production contract: [DESIGN.md](DESIGN.md).
Implementation/evidence: [IMPLEMENTATION.md](IMPLEMENTATION.md). No research-only
observation is retroactively promoted to executed VOID evidence.

## 9. Remaining engineering risks

- Private operation identity, typed IR validation or worker-effect omissions can
  accidentally expose unsafe calls; test both frontend and forged IR rejection.
- A value receiver copy passed to a free helper can discard advancement; keep
  state-changing shared helpers as mutating receiver methods or explicit returns.
- Extreme signed arithmetic, full64 width zero and float upper rounding need
  independent vectors, not just a random histogram.
- Native dependency propagation can pass JIT and fail installed Windows AOT;
  require that exact native case before claiming Windows acceptance.
- Generic copying/readonly behavior and in-place allocation failure need existing
  GC/snapshot tests. Do not promise swap rollback without evidence.
- Exact floating reproducibility inherits the existing host FP environment
  boundary. Arbitrary foreign rounding/FTZ changes are outside current contracts.
- The historical filesystem Release SIGSEGV and historical optimizer reliability
  observations remain open. This research neither diagnoses nor fixes them.
  Preserve any new abnormal source exactly before retry/reduction.


## 10. Subsequent engineering evidence

The approved implementation is now locally complete. [HANDOFF.md](HANDOFF.md)
and its linked profile receipts distinguish effective local 93/93 selection,
initial failures/corrections, independent vectors, strict environment constraints
and unverified native Windows/macOS work. Sections 1–7 above remain historical
baseline research, not retroactively claimed production execution. The original
filesystem/LLVM incidents are unchanged. Independent Golden Wrench source review
is the next stop; no hosted lock is claimed.
