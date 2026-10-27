# Golden Wrench source review handoff

**Complete local implementation; stop for independent Golden Wrench review.**
Baseline: `b374fd30aad70bd27c334150949b2a06481b1b67`.
No independent approval, hosted lock or release is claimed.

## Delivered contract

`stdlib/random.void` has **66 documented public top-level declarations**, including
**62 function/method declarations** and **110 numeric call forms**. Every written
public parameter/result has accepted documentation tags. It provides all ten
`rand_T` numeric families with exactly the approved zero/one/two-argument direct
calls, checked counterparts, Boolean, inclusive integer ranges, seeded/reseedable
value generators, lazy Runtime-local defaults, OS initialization and complete
choose/shuffle/shuffled families. [Full API inventory](API.md).

The version-1 xoshiro256**/SplitMix64 engine, signed ordinals, unbiased rejection,
precise unit-float construction, bounded float retries and Fisher–Yates remain
ordinary VOID source. No per-draw arrays/native RNG objects or crypto API exists.
[Approved contract](DESIGN.md); [attribution](ATTRIBUTION.md).

The compiler owns a canonical-random-only arity adapter and private XOR/domain/
entropy operations. Existing exact typing, contextual literals, named evaluation
order, receiver authority, function-value signatures and tooling remain in use.
A single canonical seed-parameter shadowing exception preserves the required
constructor label. Const-list slice definitions were repaired to keep physical
element types authority-free while retaining readonly child permissions.
The Runtime adds only a checked NativeSafe eight-byte OS seed bridge; Windows
bcrypt linkage is propagated to Runtime consumers and installed native flags.
[Implementation and corrections](IMPLEMENTATION.md).

## Final verification receipts

| Profile | Original consolidated run | Focused corrections/follow-ups | Effective selected areas |
|---|---:|---|---:|
| Linux Debug | 30/31 | Host/LSP corrected; one later vector timeout preserved; final compact numeric/stack 2/2 | **31/31** |
| Linux Release | 31/31 | Final compact numeric/stack 2/2 | **31/31** |
| Linux strict ASan/UBSan/leaks | 4/31 inside restricted sandbox | 27/27 affected tests passed outside sandbox; final compact numeric/stack 2/2 | **31/31** |

The effective total is **93/93 selected areas**. Exactly **130 CTest executions**
were recorded across the original runs and focused follow-ups: **101 passed,
29 initially failed** (27 sandbox LeakSanitizer infrastructure failures, one
negative-test setup error and one harness timeout). Every initially failing area
has a later passing result. This is deliberately not described as 130 green tests
or one uninterrupted all-green run.

- **792 independent vector outputs × four execution modes × normal/1 MiB stack ×
  three profiles = 19,008 exact comparisons** in the final compact fixture.
- **116 frontend processes** per normal/1 MiB profile: all types/arities, typed and
  named arguments, negatives, aliases/spoofing, ordinary resolution, readonly and
  worker admission. Typed IR independently checks XOR, malformed entropy and the
  exact forbidden worker-entropy path.
- **2,281 host assertions per profile** cover copies, replay, globals, owner checks,
  compatible reload, algorithm/layout/initializer restart, cache isolation,
  function values, GC at safepoints/swaps, checked entropy and allocation faults.
- **40 LSP checks**, **24 plain-error/cleanup processes**, **13 relocated-install/
  real-entropy processes** and **8 worker snapshot processes**, per profile.
- Independent published seed words, mixed 11-word oracle, all 256 eight-bit
  rejection residue domains, forced rejection consumption, float wire bits,
  subnormal/adjacent/opposite-extreme intervals, zero samples and fixed histograms.
  No probabilistic pass/fail gate is used.
- Linux real entropy and deterministic success/zero/EINTR/short/error/partial
  nonpublication tests; NativeSafe concurrent-GC coordination. Normal/O2 JIT,
  independently linked AOT and actual 1 MiB checks passed. Strict options retain
  leak detection and fatal ASan/UBSan behavior. Host instrumentation does not imply
  every generated JIT instruction is sanitizer-instrumented; vptr checking remains
  unavailable with the existing RTTI-off LLVM SDK.

[Effective results and all test names](evidence/FINAL.json),
[exact commands/logs/JUnit and profile receipts](evidence/),
[compiler/source/archive hashes](evidence/SOURCE_FREEZE.json),
[focused development logs](evidence/focused/index.json).
Final fixture SHA-256: `13f0331f5e59fc7462e4f7234366de663fa24f5269a94f66a4ad77294cc697ca`.
Independent expected-output SHA-256:
`36e56674f9715a31baa2cfee1245056faca1ec27b5143c56200d063aa19fe8d1`.

## Incidents, limits and owner stop

[Preserved incident index](INCIDENTS.md) distinguishes **two checked internal IR
rejections**, **nineteen sandbox LeakSanitizer reserved-status exits** and **one
180-second timeout/SIGKILL**. Copied source/provenance hashes were verified.
Original failures remain available; no SIGSEGV, Windows exception, LLVM
miscompilation or repaired historical reliability issue is claimed. Legacy direct
C++ test provenance limits are explicit in that index. Existing historical
filesystem/LLVM crash research is unchanged.

Linux native results do not establish **Windows BCryptGenRandom/Bcrypt.lib** or
**macOS getentropy** success. Those branches and Windows Debug/Release installed
linkage still need genuine native execution before any such acceptance claim.
There was no staging, commit, branch, push or hosted CI. Root README bytes still
match the baseline (`c287f35842b03a1c5ac170718e674cc5498f877ade76eede5273859921d62add`).

Golden Wrench should review the actual source and retained evidence, particularly
the narrow canonical adapter, the seed-label exception, authority projection,
state/consumption rules, native failure publication and preservation boundaries.
Boss retains Git, native-platform disposition, hosted CI and release authority.
**Engineering stops here for independent source review.**
