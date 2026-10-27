# Golden Wrench handoff — await/result syntax

27 October 2026. Started from clean HEAD `9a2488972d4c2674317492f2640d3b8c7357fb11`.
Implementation and local gates are complete; ready for independent Golden Wrench
source/evidence review. **Hosted acceptance remains pending Boss CI.**

**STATUS: COMPILER STABILIZATION COMPLETED / STANDARD-LIBRARY DEVELOPMENT HOLD LIFTED BY BOSS.**

Boss explicitly authorized lifting the special stabilization freeze after the
new syntax passed local acceptance. That condition is satisfied in this delivery.
Normal VOID development may resume under the existing Boss approval, Research,
Wrench review and Keyboard implementation workflow. This grants no unspecified
module or unrelated feature approval. This sprint begins no standard-library
work and introduces no standard `error` module.

## Parser and compatibility

The sole production change is in `src/frontend/compiler.cpp`, in the existing
`postfix`, `primary` and `unary` parser routines. A propagation flag lets the
unparenthesized await operand consume calls, members and indexes but stop before
`?`. The await node is then completed through the ordinary postfix parser.
Parentheses, call arguments, array elements and index expressions enter the
ordinary `expression()` boundary with propagation enabled. No AST rewrite or
new representation of parenthesization is needed.

| Source | Grouping |
|---|---|
| `await job?` | `(await job)?` |
| `(await job)?` | `(await job)?` |
| `await (job?)` | `await (job?)` |
| `await load()?.field` | `((await load())?).field` |
| `await nested()??` | `((await nested())?)?` |
| `await job? + 1` | `((await job)?) + 1` |

Existing await/task, exact result error compatibility, lowering, IR validation,
Runtime, scheduler, cleanup, GC/ownership and cache mechanisms are unchanged.
The `await` and `?` nodes retain their original tokens and source spans. No
opcode, task layout, diagnostic ID, depth limit or editor-specific grammar is
added. The previously parenthesized spelling remains compatible.

Seven pairs independently check and lower the sugar and parenthesized forms,
then compare source-location-independent typed CFG signatures. These cover
handles, calls, spawn, grouping, arithmetic and multiple awaits. A valid
`await (envelope_job?)` unwraps a result containing a task; propagating an ordinary
task inside those parentheses is rejected at its written `?`.

## Local acceptance

| Profile | Passed | Failed | Driver wall seconds |
|---|---:|---:|---:|
| release | 131/131 | 0 | 267.74 |
| debug | 131/131 | 0 | 1004.51 |
| san | 130/130 | 0 | 893.48 |

Source and executable/archive hashes are stable before/after every final gate.
All 82 selected build targets are freshly rebuilt in each profile.

Each source suite has **14 positive source/CFG groups, 12 expected rejected
cases, six malformed/recovery cases and 20 JIT/GC executions**, using both normal
and O2 IR policies and both all-export and entry emission. It runs once with the
default stack and once with a hard 1 MiB stack in each profile. Across those six
runs: 120 successful execution checks, 72 expected negative cases, 36 malformed
cases, and **2,568 injected allocation failures** (214 per IR mode per source
suite). Deliberate `Err`, cancellation and runtime failures are expected outcomes,
not failed tests.

The five execution fixtures cover receiver and argument order/exactly-once
execution, real suspension/resumption, nested async calls, repeated awaits,
managed identity, independent fixed-value copies, nested `??` with both inner and
outer errors, member/index chains, `?` inside arguments/indexes, managed derived
errors with exact E, group cancellation/draining, resources and reached defers.
Cancellation and division failure remain distinct checked outcomes and each runs
all three reached cleanups (observed ledger 111). Early `?` preserves the expected
resource/defer ordering (234) and cancels/drains the live sibling exactly once.

Precise-root probes force collection before construction and check registered
roots/commits and balanced frames. After each invocation/fault attempt, collection
returns managed object counts to zero, after the test observer releases its own
root. Allocation sweeps finish successfully after all earlier injected failures.
Exact derived-to-base E mismatch rejects at source `?` in both spellings; ordinary
managed object upcasting still checks. Existing async independent-IR rejection,
lifecycle, worker, callback, reload and GC tests remain in the regression gate.

Each of the ten AOT fixture/mode combinations independently checks source with
`voidc`, re-emits its object under a **hard 1,048,576-byte soft and hard stack
limit**, compares those bytes with the linked object and runs the Runtime-only
native executable under the same limit. This runs in all three profiles. No JIT
serves as the AOT execution oracle.

The new LSP process uses the same hard 1 MiB limit and checks projected hover,
definition, version-safe rename, documented-parameter signature help, formatting,
member completion after `await call()?.`, exact question-mark diagnostic spans,
malformed-source recovery and repaired overlays. Existing LSP, C3 chain/depth,
source CLI and inference regressions are retained. The sanitizer selection keeps
the stabilization gate's existing exclusion of `lsp_const_recovery_stack`; all
new syntax stack, AOT and LSP tests run under strict sanitizers.

Twenty independent CLI checks compare all five old/new fixture forms with the
frozen starting compiler on hard 1 MiB stacks. All parenthesized controls pass
both compilers; the five new forms fail source checking in the starting compiler
and pass in this delivery. The two unmatched-delimiter controls preserve identical
baseline diagnostics; later-declaration recovery is asserted for balanced damage.

## Stabilization and cache guard

Same source bytes, pinned LLVM 19.1.7 and Default native codegen. Each
measurement is a serial fresh child with a hard 1 MiB stack, after all builds and
acceptance tests finish. Three uncached default/O2 samples per case; no OS cache
flush is claimed. Desktop services remain active; wall/CPU/RSS/load are retained.

| Case | Stabilized starting compiler median wall s | Syntax compiler median wall s |
|---|---:|---:|
| dto128 | 8.423566 | 8.674363 |
| import-json | 0.234346 | 0.237565 |
| gameplay | 0.066688 | 0.062364 |
| tiny | 0.024280 | 0.024063 |

- `baseline-dto128-uncached-no-IR-O2`: 6.455248 s wall, 6.405422 s CPU, 145440 KiB peak RSS; status 0.
- `syntax-dto128-uncached-no-IR-O2`: 6.448638 s wall, 6.384823 s CPU, 147640 KiB peak RSS; status 0.
- `syntax-dto128-auto-cold`: 8.433252 s wall, 8.363816 s CPU, 138340 KiB peak RSS; status 0.
- `syntax-dto128-auto-warm-1`: 0.452840 s wall, 0.443982 s CPU, 57880 KiB peak RSS; status 0.
- `syntax-dto128-auto-warm-2`: 0.443339 s wall, 0.439663 s CPU, 58764 KiB peak RSS; status 0.
- `syntax-dto128-auto-warm-3`: 0.436661 s wall, 0.433101 s CPU, 57840 KiB peak RSS; status 0.
- `syntax-dto128-AOT-build`: 7.637935 s wall, 7.587704 s CPU, 135720 KiB peak RSS; status 0.
- `syntax-dto128-AOT-native-0`: 0.268837 s wall, 0.265998 s CPU, 13472 KiB peak RSS; status 0.
- `syntax-dto128-AOT-native-1`: 0.262257 s wall, 0.259153 s CPU, 13472 KiB peak RSS; status 0.
- `syntax-dto128-AOT-native-2`: 0.262360 s wall, 0.259357 s CPU, 15976 KiB peak RSS; status 0.

All **38 measurements pass, zero failures**. The large cold-compile
improvement remains intact. These local observations are regression evidence,
not a cross-machine timing promise. Full measurements retain source/image hashes.

The existing stabilization selection is retained and expanded to all 34 existing
async AOT combinations plus the 13 new syntax/editor tests. It includes JSON
work-budget ordering, generic projection/readonly checks, arithmetic and rejected
chain stack regression, operators, globals, GC, native callbacks, reload and
source/application/editor paths. Automatic optimized caching, no-IR-O2 separation,
`--no-cache`, source/import invalidation, corruption/admission and concurrency
regressions pass. A separate cross-compiler cold/warm check verifies compiler-key
invalidation and unchanged warm object files. Cache policy and automatic behavior
are unchanged; a new compiler identity correctly gets a new cache entry.

Runtime headers and all three Runtime archive hashes match the starting snapshot.
The protected root README hash remains
`c287f35842b03a1c5ac170718e674cc5498f877ade76eede5273859921d62add`.
No standard-library source, backend, scheduler or IR implementation changed.

## Modified-file inventory and evidence

| File | Scope |
|---|---|
| `CMakeLists.txt` | Test registration/build integration |
| `docs/ASYNC.md` | Async syntax contract/examples |
| `docs/KNOWN_ISSUES.md` | Current Boss status and retained historical record |
| `docs/LANGUAGE_BIBLE.md` | Async syntax contract/examples |
| `docs/README.md` | Current Boss status and retained historical record |
| `docs/WRENCH_CLANKER.md` | Current Boss status and retained historical record |
| `docs/async-await-syntax/HANDOFF.md` | Independent review handoff |
| `docs/async-await-syntax/evidence/README.md` | Frozen local evidence |
| `docs/async-await-syntax/evidence/SHA256SUMS` | Frozen local evidence |
| `docs/async-await-syntax/evidence/acceptance-debug-1793128768963391983.json` | Frozen local evidence |
| `docs/async-await-syntax/evidence/acceptance-debug-1793128768963391983.txt` | Frozen local evidence |
| `docs/async-await-syntax/evidence/acceptance-debug-1793128768963391983.xml` | Frozen local evidence |
| `docs/async-await-syntax/evidence/acceptance-release-1793128469287261569.json` | Frozen local evidence |
| `docs/async-await-syntax/evidence/acceptance-release-1793128469287261569.txt` | Frozen local evidence |
| `docs/async-await-syntax/evidence/acceptance-release-1793128469287261569.xml` | Frozen local evidence |
| `docs/async-await-syntax/evidence/acceptance-release-1793128768959882322.json` | Frozen local evidence |
| `docs/async-await-syntax/evidence/acceptance-release-1793128768959882322.txt` | Frozen local evidence |
| `docs/async-await-syntax/evidence/acceptance-release-1793128768959882322.xml` | Frozen local evidence |
| `docs/async-await-syntax/evidence/acceptance-san-1793128768967217121.json` | Frozen local evidence |
| `docs/async-await-syntax/evidence/acceptance-san-1793128768967217121.txt` | Frozen local evidence |
| `docs/async-await-syntax/evidence/acceptance-san-1793128768967217121.xml` | Frozen local evidence |
| `docs/async-await-syntax/evidence/attempts.json` | Frozen local evidence |
| `docs/async-await-syntax/evidence/baseline-sources.tar.xz` | Frozen local evidence |
| `docs/async-await-syntax/evidence/compatibility.json` | Frozen local evidence |
| `docs/async-await-syntax/evidence/current-sources.tar.xz` | Frozen local evidence |
| `docs/async-await-syntax/evidence/inventory.json` | Frozen local evidence |
| `docs/async-await-syntax/evidence/manifest.json` | Frozen local evidence |
| `docs/async-await-syntax/evidence/performance-measurements.jsonl` | Frozen local evidence |
| `docs/async-await-syntax/evidence/performance-summary.json` | Frozen local evidence |
| `docs/async-await-syntax/evidence/regression-receipts.json` | Frozen local evidence |
| `docs/async-await-syntax/evidence/regression-receipts.tar.xz` | Frozen local evidence |
| `docs/async-await-syntax/evidence/reproduction.tar.xz` | Frozen local evidence |
| `docs/compiler-health/ISSUES.md` | Current Boss status and retained historical record |
| `docs/compiler-health/REPORT.md` | Current Boss status and retained historical record |
| `docs/compiler-health/ROADMAP.md` | Current Boss status and retained historical record |
| `docs/compiler-health/stabilization/AUDIT.md` | Current Boss status and retained historical record |
| `docs/compiler-health/stabilization/HANDOFF.md` | Current Boss status and retained historical record |
| `docs/compiler-health/stabilization/IMPLEMENTATION.md` | Current Boss status and retained historical record |
| `src/frontend/compiler.cpp` | Compiler-owned precedence |
| `tests/async/syntax.cpp` | Async syntax regression |
| `tests/async/syntax/cancelled.void` | Async syntax regression |
| `tests/async/syntax/errors.void` | Async syntax regression |
| `tests/async/syntax/failure.void` | Async syntax regression |
| `tests/async/syntax/spawn.void` | Async syntax regression |
| `tests/async/syntax/values.void` | Async syntax regression |
| `tests/async/syntax_aot.py` | Async syntax regression |
| `tests/async/syntax_lsp.py` | Async syntax regression |
| `tests/async/tests.cpp` | Async syntax regression |

The exact file/hash inventory is in [inventory.json](evidence/inventory.json).
[Evidence manifest](evidence/manifest.json), [checksums](evidence/SHA256SUMS),
[compatibility controls](evidence/compatibility.json), acceptance XML/JSON/logs and
compressed reproduction sources/helpers retain input/build identities and all
attempts. The original compiler-stabilization evidence packet is unchanged.

## Failed attempts and review boundary

The first full Release run passed 130/131 with one test-fixture failure: signature
help correctly returned no undocumented parameter under the existing accepted
policy. Documenting that fixture parameter corrected the expectation; the complete
frozen final run passes. No product behavior or test gate was weakened. Initial
fixture-authoring failures (name shadowing, a grouped result's expected payload,
rename capability negotiation, unmatched-delimiter recovery, the baseline nesting
limit and a missing Runtime attachment in the GC observer) are retained with the
corrected tests. Old generated Makefile invocations initially lacked the newly
added targets; fresh final builds rebuild all 82 selected targets in each profile.
These are distinct from product crash findings.

**H1–H7 remain unresolved historical incidents with their existing classifications
and evidence.** Passing tests and lifting the special development hold do not
claim historical repair, independent acceptance, native Windows/macOS execution,
or hosted CI acceptance for this syntax.

Golden Wrench should review the parser boundary and explicit grouping, task/result
projection, diagnostic source tokens, cleanup/ownership fixtures, stack evidence
and correspondence of frozen sources/builds/receipts. Boss retains staging,
branches, commits, pushes, hosted CI and release acceptance. No such Git mutation,
hosted dispatch, protected README edit or ptrace-based debugging was performed.

THREE CLANKERS. ONE VOID. STRONG BOLTS. NO GHOST BOLTS.
