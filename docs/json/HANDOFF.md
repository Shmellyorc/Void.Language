# JSON implementation handoff

27 October 2026. Baseline `194603ea00eb38aff72b7d6e950865b3a23df92a`.

**Implementation and focused Linux local verification are complete. Stop for
independent Golden Wrench source review.** Boss retains Git, hosted CI
and release control. No staging, commit, branch, push or hosted CI was performed.

## Delivered behavior

The complete requested dynamic and automatic typed API is implemented in
[`stdlib/json.void`](../../stdlib/json.void). Public API documentation is in
[API.md](API.md); executable examples are [save.void](../../examples/json/save.void)
and [validated.void](../../examples/json/validated.void).

Boss resolved the imported `value`/loop-binder conflict by choosing
`use json as json`. Examples with `for value, index in levels` use that import.
The existing no-shadowing, interpolation and enumeration grammar remain intact.

There is one ordinary source parser and one compact source writer. Both use
explicit heap frames. Numbers retain their exact validated lexemes; integer
conversion normalizes decimal digits and exponent without an f64 intermediate.
Floating conversion uses the existing matching-width primitive conversion and
display. Decoded duplicate names, malformed JSON/Unicode, nonfinite typed values,
missing fields, null, wrong kinds and resource exhaustion remain distinct.

The private dynamic representation admits no public mutation or construction.
Observations retain child identity and readonly authority. Array/key observations
copy the outer collection. Both node conversion directions produce fresh trees;
the writer currently also uses that fresh-tree conversion for `json.value`.

## Compiler scope and shared repairs

[`src/frontend/json.inc`](../../src/frontend/json.inc) admits a bounded concrete
type graph and generates ordinary source, parsed into the existing AST and checked
through normal semantic and independent IR validation. Admission happens before
adapter-cache lookup. Direction, concrete type/capabilities and `json-v1` distinguish
adapter identities. Existing compiler/library/native-cache contracts remain active.

Per-type owner/result stacks and a dispatch loop provide postorder conversion and
ordinary named construction. Required references and const fields exist before
their parent is constructed. Fixed arrays use the existing independently checked
per-element construction recipe. Collection snapshots use ordinary checked source
enumeration. Wide structs use an ordinary construction closure over completed
typed stacks, adding constant native call depth. No recursive call per document
edge, new IR opcode, reflection API, Runtime object walker, native JSON dependency
or access-privilege exception was introduced.

Plain public stored-data DTOs include concrete generics, value/managed forms,
effective base fields and nested stored composition. Unsupported storage,
private/internal fields, custom init/deinit, property-only schemas, payload enums,
results, interfaces, callables, tasks and resources diagnose. Invariant-bearing
application types use an explicit DTO and existing factory; the validated example
executes that route.

Confirmed shared frontend/lowering repairs are deliberately separate from JSON:

- Growable handles use ordinary checked stores for capture/enumeration; raw
  fixed-array capture retains its existing value-storage role.
- Pattern temporary/narrowed annotations follow inferred readonly facts.
- A value-pattern RHS is rooted using its own authority profile, preserving
  comparison with readonly aliases without changing independently held aliases.
- Error recovery restores the pattern-binding flag with RAII; the preserved
  invalid `is not` reduction now returns VOID2001 instead of aborting.

`src/ir/reference_capabilities.inc`, Runtime, backend and sanitizer flags were not
changed. The original six research incidents remain in the unchanged archive.

## Bounds and cleanup

Defaults are 16 MiB input/output, depth 64 (hard maximum 128), 262144 logical nodes,
8 MiB per decoded string/key, 1024-byte numbers, 4096 object members and 67108864
conservative work units. Every setting must be positive. Compound operations
share counters. These are logical bounds, not RSS or wall-clock quotas.

Work is checked before input copies, scans, output expansion, numeric normalization,
duplicate comparisons, traversal, dictionary hashing/probing/reindexing and growth.
Dictionary charging follows the actual zero initial capacity and first growth to
four entries. Ordered duplicate comparison is deliberately bounded quadratic work.
Cycles and excessive acyclic depth both terminate through ordinary limit errors.
Source allocations retain normal Runtime failure handling and precise GC roots.

## Verification inventory

The final selection contains **31 CTests per profile: 18 JSON and 13 affected
shared groups**. Profiles are Linux Debug, Release and RelWithDebInfo with strict
ASan/UBSan/leak settings. CLI/check/JIT/build/native and host children use actual
1 MiB stack limits; LSP additionally inspects the running child's `/proc` limit.

| Profile | Final consolidated selection | Verified replacements | Effective current groups |
|---|---:|---|---:|
| Debug | 30/31 | Corrected adversarial driver; updated wide-GC host | 31/31 |
| Release | 30/31 | Corrected adversarial driver; updated wide-GC host | 31/31 |
| Strict ASan/UBSan/leaks | 31/31 | None | 31/31 |

**93/93 effective groups pass; zero unresolved JSON-origin internal diagnostics,
native crashes or sanitizer failures remain.** The two consolidated source-error
failures are retained; the corrected adversarial cases pass all four modes in
separate recorded runs. Stable-binary host replacements include the added wide-GC
probe. No failed receipt is replaced or relabeled as a pass.

The six consolidated selections contain 165 CTest group executions: 156 passes
and nine retained failed attempts (seven from the earlier matrix, two from the
nested-propagation driver). Four targeted replacement groups pass, giving 169
group executions and 160 passes across this explicitly counted selection. These
are attempt counts, separate from the 93 effective current groups; earlier
exploratory commands and benchmark/example runs are not included in that total.
The save example additionally passes an actual source check and exact-output run;
its bytes match the typed fixture covered in all four modes.

[Machine-readable results and every cohort/replacement receipt](evidence/implementation/verification-summary.json).

| JSON group | Reviewed cases per profile |
|---|---|
| Readonly dependency | 47 process checks, including four-mode positives and 11 language-error negatives |
| Grammar/Unicode | 100 vectors in JIT O0/O2 and independently linked AOT O0/O2 |
| Numbers | 125 vectors in all four modes |
| Schema | 14 ordered error/success vectors in four modes, 21 admission/authority negatives |
| Bounds | 56 vectors in all four modes |
| Adversarial | 22 vectors in all four modes: typed depth, colliding keys, aliases, ten scalar adapters, decoded controls/surrogates |
| Execution | Six fixtures, four execution modes and two native builds each |
| Large stack workloads | Fixed 4096 values and a 128-field DTO, each in all four modes |
| Relocated install | Seven process checks against installed compiler/library bytes |
| LSP | 43 checks: docs/navigation/references, chains, UTF-16, unsaved/stale overlays, incomplete calls, privacy and protected rename |
| Native host | Forced collection, allocation failures/retries, cleanup, O0 native-object cache hit, fresh Runtime, old-generation lease and reload rejection |
| Preservation | Existing disposable three-incident preservation test |

The native host additionally forces GC through the new construction closure with
32 required public const managed children, in both optimization modes. Its receipt
prints root observations, managed constructions and poll counts. The scalar/save
sweep injects 2050 allocation failures per mode before success, checks a successful
retry after each failure, then checks zero temporary objects and zero handles.
Each updated profile passes **12,343 host assertions** (37,029 across profiles).
Per mode and profile, the wide const probe observes 223,824,205 roots, 266 managed
constructions and 50,564 polls; the save/parse/error probe observes 12,087,416 roots,
85 managed constructions and 11,005 polls. The 12,300 allocation failures injected
across all profiles/modes are intentional and already represented in those checks.

The existing native-object facility is `DebugCache`: optimized compilation bypasses
it. Cache-hit checks therefore use O0, including separate fresh applications; GC
and reload checks exercise both optimization modes. JSON does not expand cache scope.

Strict environment:

```text
ASAN_OPTIONS=detect_leaks=1:halt_on_error=1:abort_on_error=0:exitcode=87
UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1:exitcode=86
```

Run `python3 tests/json/acceptance.py debug` (or `release`/`san`) from the repository
root after building the selected targets. Each attempt writes timestamped JUnit,
JSON and log receipts with configuration, source/build hashes, commands and status.
The established text-encoding/Random preserving boundary retains abnormal output,
timeouts, loaded inputs and stdlib bytes before cleanup or retry.

Zero-argument `as_number` signature help follows the existing documented editor
contract: no active documented argument yields null help. Hover and definition
still work; the JSON implementation adds no editor exception or fake parameter.

## Historical attempts and provenance

Original consolidated selections were Debug 22/24, Release 22/24 and strict 21/24:
65 passes and seven failed group attempts, retained separately from final outcomes.
Failures were compilation/group timeouts for shapes and wide stress, plus a test
assertion expecting a JSON-specific diagnostic for a task already rejected by
ordinary generic argument checking. These are not reported as native crashes.

Wide stress originals/reductions and owning-debugger diagnostic interruptions are
retained. Source checking succeeds; LLVM machine scheduling dominates large-schema
compilation. The construction closure produces the exact wide output at O0, measured
at 246.61 seconds; shapes O2 produces exact output at 148.76 seconds. Stress cases
are split and allow 600 seconds per compilation; the fault sweep allows 1500
seconds within its existing 1800-second CTest limit. Actual 1 MiB stacks, all four
modes and all sanitizer/optimizer flags are preserved. Timings are observations.

Two Debug/Release host rows began before the additional wide-GC test was built.
Their post-run executable hashes cannot establish which unlinked binary ran.
They are excluded as final wide-probe evidence and replaced by stable-binary runs
in separate fixture directories. See `host-observation-source-race-audit.json`.
The improved wrapper hashes executable/library inputs before execution and asserts
their stability afterward. Final strict host started after the rebuilt probe.

Earlier baseline reproductions, the invalid-pattern abort, readonly identity IR
failure, CLI/test-expectation mistakes and all retries remain documented in
[IMPLEMENTATION.md](IMPLEMENTATION.md) and the evidence directory. No historical
Math, Random, filesystem or optimizer incident is closed by JSON success.

## Performance observations and limits

Warmed Release native O2 measurements exclude compilation. Each row is 20
iterations; stringify includes the fresh DOM conversion used by the public API.

| Corpus | Bytes / nodes | Parse total | Stringify total |
|---|---:|---:|---:|
| Save | 125 / 12 | 71.11 ms | 46.16 ms |
| Config | 90 / 9 | 48.54 ms | 34.62 ms |
| Depth 64 | 129 / 65 | 258.76 ms | 271.17 ms |
| Width 128 | 1445 / 129 | 2736.01 ms | 526.67 ms |

These measurements have no performance pass threshold. Large generated schemas
can take minutes to compile in LLVM 19.1.7; the width fixture makes that limitation
explicit. Bounded duplicate checking is intentionally not a hash-based parser.

Native Windows and macOS ARM64, their installed/AOT/stack behavior and hosted CI
remain **unverified**. Strict builds instrument the C++ compiler/Runtime/host;
generated LLVM application instructions do not acquire an extra ASan pass.
The existing frontend's inferred generic arguments can select a result wrapper
when given a nested propagated expression, such as `stringify(parse_as<t>(s)?)`.
The unrelated `identity(read()?)` reduction reproduces this with the frozen compiler.
Bind the payload to a local first; the approved sequential examples do this.
The expanded scalar fixture was corrected to use that route, with the failed
source diagnostics and a hash-matching regenerated reproduction retained.
Independent Golden Wrench source review and Boss-controlled release acceptance
remain outstanding.

## Changed files and hashes

Production changes: `CMakeLists.txt`, `src/frontend/compiler.cpp`,
`src/frontend/capabilities.inc`, `src/frontend/pattern_lowering.inc`, new
`src/frontend/json.inc`, new `stdlib/json.void`, and `stdlib/README.md`.
Documentation, executable examples, tests, reductions and retained evidence live
under `docs/json/`, `examples/json/` and `tests/json/`.

`docs/README.md`, `docs/json/` and `docs/math/evidence/reruns/` were already dirty
at entry. The pre-existing docs index and Math reruns were not edited by this task.
Root `README.md` and the independent IR validator remain unchanged.

The [final inventory](evidence/implementation/final-inventory.json) records all
implementation-owned files, retained evidence and SHA-256 source/build identities.
[Source freeze 10](evidence/implementation/source-freeze-10.json) records all 177
production/library/test files; the final inventory records completed documentation
separately and excludes itself from hashing. It also records the unchanged baseline,
protected root README, independent validator, original research archive and initial
dirty-file boundaries. Full CMake configurations and strict environments remain
in the timestamped acceptance receipts.
