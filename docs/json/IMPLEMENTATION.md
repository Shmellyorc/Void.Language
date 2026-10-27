# JSON implementation recovery record

27 October 2026. Boss authorized all three research decision bundles and the
complete DESIGN/ROADMAP in the attached implementation request. Earlier proposal
and approval-gate wording in research documents is historical. No additional
feature approval is needed for ordinary continuation.

Baseline HEAD: `194603ea00eb38aff72b7d6e950865b3a23df92a`.
Initial dirty work: `docs/README.md`, `docs/json/`, and
`docs/math/evidence/reruns/`. No AGENTS.md exists in this workspace or its parent.
Root README, Git index/branches/history/remotes, hosted CI, historical incidents
and Math reruns are protected. Original six research incidents remain in the
unchanged `evidence/capability-probes.tar.gz`.

## Completed local verification

The complete approved JSON implementation and focused Linux checks are complete.
Final effective outcomes are 31/31 each in Debug, Release and strict ASan/UBSan/leaks:
93 current groups, with zero unresolved JSON-origin internal diagnostics, native
crashes or sanitizer failures. Debug/Release use recorded adversarial and stable-host
replacements; strict's consolidated selection passes 31/31. All updated hosts pass
12,343 assertions, including forced GC through 32 required const managed children.
JIT O0/O2, independently linked AOT O0/O2, actual 1 MiB stacks, relocated install,
LSP, resource/Unicode/numeric/schema vectors, cache/reload and cleanup are covered.

The six consolidated selections retain 165 group attempts (156 pass, nine fail).
Four successful targeted replacements yield 169 counted group attempts, 160 passes,
and 93/93 effective current groups. Earlier exploratory runs remain separately
recorded. [Verification summary](evidence/implementation/verification-summary.json),
[completed HANDOFF](HANDOFF.md) and [final inventory](evidence/implementation/final-inventory.json)
contain the receipts, exact counts, source/build hashes and limitations.

Remaining boundaries are documented: existing no-shadowing and nested-propagation
generic-inference rules, managed-reference nullable syntax, slow compilation of
large schemas, and the approved public-data/static-schema conversion scope. Native
Windows/macOS and hosted CI are unverified. Root README, independent IR validator,
original research incidents and Math reruns remain preserved. No Git mutation or
hosted CI was performed. **Stop here for independent Golden Wrench source review.**

## Historical engineering checkpoints

The entries below record the state at each checkpoint. The latest acceptance
receipt and HANDOFF supersede their pending-work and approval statements.

### Initial progress

- Step 1: contract accepted; initial source/build/cache hashes and dirty state
  retained in `evidence/implementation/initial.json`. Current Debug, Release and
  strict binaries copied to ignored `build/json-baseline/` before any source edit.
  The pre-edit Debug build target was confirmed up to date.
- Step 2: baseline reproduction and reductions retained in
  `evidence/implementation/readonly-baseline.json` and
  `readonly-reduction.json`; new internal failures preserved with the existing
  text-encoding crash wrapper. These status-1 IR rejections are not signals.
  Initial baseline script mistakenly used unsupported `--opt`; all four status-2
  CLI errors are retained. Follow-up uses normal default O2 and `--no-opt` O0.
- Reduction: plain managed child retrieval through a list reproduces without
  results. Lowering uses raw `array_capture` for a growable alias, imposing a
  mutable-authority obligation absent from frontend source effects. An alternative
  safe `.get` route exposes stale pattern operand types after readonly inference.
- Candidate repair: ordinary local stores for growable captures/iteration;
  fixed-array capture remains as before. Propagate subject capability profiles to
  pattern temporary/narrowed types. Independent IR validator is unchanged.
  Verification is in progress; this is not a completed repair claim.

## Ordered remaining work

2. Finish positive readonly Ok/?/child/forwarding/collection tests, negative
   mutation and authority escape tests, independent malformed-IR tests and
   Debug/Release/strict regressions.
3. Source module/errors/limits/private representation and owner construction.
4. Shared bounded byte primitives, JSON escapes/numbers/error locations.
5. Iterative parser and decoded duplicate handling.
6. Observations and exact numeric normalization/conversion.
7. One iterative compact writer.
8. Concrete type admission with source diagnostics and invariant exclusions.
9. Iterative compiler-generated to-value adapters through normal checking.
10. Iterative from-value adapters, postorder ordinary named construction.
11. Public wrappers sharing contexts and canonical-private binding.
12. Dictionary work accounting, mutation/GC/failure cleanup.
13. LSP/install/cache/reload integration.
14. Full focused acceptance and final HANDOFF with counts, source/build hashes,
    changed-file inventory and explicit platform limits. Stop for Golden Wrench.

The full approved ROADMAP remains binding. No production JSON implementation,
completed test matrix, independent source review or platform acceptance is claimed.

## Continuing engineering checkpoint

Readonly positive/negative selections pass 40/40 in each of Debug, Release and
strict sanitizers (120 checks, including JIT O0/O2 and independently linked AOT
O0/O2 with verified 1 MiB pre-exec stack limits). Reports are `readonly-debug.json`,
`readonly-release.json`, and `readonly-san.json`. These precede subsequent numeric
and adapter compiler changes and must be rerun against the final source. Release
shared regressions pass 5/5; Debug selection is running. IR validation is unchanged.

An invalid `is not` expression in the developing helpers exposed an existing
compiler SIGABRT in error recovery. Both the original whole source and a reduced
baseline reproduction are retained in incidents. GDB traces identify the lost
`pattern_bindings` flag and wrong scope removal. The expression checker now
restores this flag with RAII; the invalid reduction returns the expected type
error instead of a signal. JSON source uses the existing `not (x is case)` syntax.
No claim about historical unrelated crashes follows.

The source module now contains public types/errors/limits, private owner-built
payloads, iterative parsing, observation, exact decimal normalization, one iterative
writer, source clone and typed traversal helpers. The Unicode/NUL, readonly
observation and dynamic enumeration smoke fixture executes. These are developing
implementations, not accepted JSON correctness claims.

`src/frontend/json.inc` contains developing type admission and ordinary checked
iterative adapters, including existing fixed-array per-element recipes rather than
extent-sized native frames. No new IR opcode, Runtime walker, dependency, general
reflection or visibility exception was added. Typed smoke checking found generated
empty-map syntax and enum-alias selection issues; repairs are being verified.
No typed execution pass is yet claimed.

A confirmed contract conflict requires owner direction: public `json.value` is
imported unqualified by `use json`; the existing no-shadowing rule rejects the
requested `for value, index in levels`. The frozen baseline reproduces with a
minimal unrelated module exporting `struct value`. Explicit `use json as json`
executes the same loop. Exact evidence is `import-binding-conflict.json`, and both
fixtures remain under `tests/json/import_binding/`. An asynchronous question asks
whether to retain the rule and use an explicit import alias, or authorize a shared
loop-binder shadowing change. No language-rule change has been made. Other feature
implementation continues independently while that required direction is pending.

## Latest recovery checkpoint

The first typed smoke and expanded shape executions pass, covering the requested
list and dictionary experiences with explicit `use json as json`, concrete generic
DTOs, nullable recursive links, fixed arrays including required managed elements,
public const fields, inheritance, stored composition and fresh DOM conversion.
Grammar: 44 reviewed vectors in all four modes passed. Numeric: 115 vectors in all
four modes passed. Bounds: 35 vectors in all four modes passed. Schema: 14 ordered
error vectors in JIT O0/O2 plus nine compile-time exclusions passed. These developing
runs precede final source freeze and are retained separately from final acceptance.

LSP: 28 checks pass, including original docs/navigation, UTF-16 positioning,
private-name filtering, incomplete generic module calls and installed rename denial.
The initial zero-argument `as_number` signature-help assertion was a test expectation
error: the locked DOCUMENTATION_HOVER contract suppresses help without an active
documented argument, including zero-argument methods. Its hover/definition work;
no placeholder parameter or exception to that contract was added. Relocated install
passed seven processes, including installed JIT and independently linked O0/O2 AOT.

Ownership testing exposed a further shared readonly lowering defect in comparisons:
`if writable is readonly_alias` roots the RHS using the subject's mutable type. The
frozen baseline reproduces without JSON. Its original incident and reduction are
retained. `pattern_lowering.inc` now snapshots the RHS in its own type. The JSON
ownership smoke then passes, retaining child identities, cloning root/children,
allowing independent outer-list edits and preserving source data. A source regression
also verifies independently held mutable aliases remain writable.

Two native-host test expectations required correction, with original failed receipts
retained: 2048 injected allocation budgets all legitimately failed before the first
success (the extended selection reaches success at 8192, after 2050 failing budgets),
and successive generations in one Jit intentionally share a Runtime. Fresh application
is now tested with a separate Jit/cache load. Actual operation results are checked
outside the fault-catching block so an incorrect result cannot count as expected OOM.
Final host runs, strict sanitizer verification and precise-object cleanup are pending.

Generated growable-list traversal now snapshots through ordinary checked enumeration,
as dictionary traversal already did. Fixed arrays retain ordinary value snapshots.
Field-origin diagnostics retain original declaring-source spans, including inherited
and nested unsupported fields. These compiler edits require fresh final builds.

Work-accounting audit corrected DESIGN's initial dictionary capacity: native header
capacity is zero, first growth is four. That first bucket clear/reindex is precharged.
Source primitives additionally precharge UTF-8 slice/decode copies, complete control
escape expansion, bounded index formatting and conservative numeric normalization.
No Runtime or independent IR validator code was changed.

Current continuation: freeze the final sources, rebuild Debug/Release/strict targets,
run the focused JSON and affected shared suites, repair actual failures and retain
all attempts. Add full acceptance/hash inventory and HANDOFF only with honest counts.
The import/binder conflict question remains pending; no shadowing rule has changed.
Native Windows/macOS remain unverified; no hosted CI or Git mutation is authorized.

## Owner resolution: import alias

Boss answered the preserved conflict question: **Keep the language rule; use
`use json as json` in examples.** The exact list-enumeration experience now uses
that approved import form. This resolves the contract conflict; no compiler
shadowing change or further approval gate is required. Continue all local
verification, then stop for independent Golden Wrench source review.

## Final-matrix continuation: wide DTO scheduling

The first consolidated Debug/Release/sanitizer attempts are retained rather than
replaced. The combined 4096-element fixed/128-field DTO stack stress timed out at
300 seconds in Release and Debug; exact original sources and stdlib bytes are in
new incident receipts. Full source checking succeeds in under one second. The
fixed-only reduction executes successfully in about 54 seconds. An owning GDB
session (explicit diagnostic SIGINT, not a crash) places wide-only execution in
LLVM's machine scheduler, building chain dependencies. Direct live attachment was
not permitted; that failed optional diagnostic is retained too.

A candidate generator-only repair outlines postorder construction for structs with
at least 32 fields into an ordinary checked closure. It captures completed typed
result stacks and returns before the traversal loop continues. Native call depth
remains constant. Compiler/backend/IR/sanitizer flags and validators are unchanged.
Object rebuilds for all three profiles are in progress; new binaries must not be
claimed verified until the original attempts finish and fresh runs complete.

The supplemental task exclusion expected a JSON admission diagnostic, but ordinary
generic inference rejects a task argument with VOID2001 before the adapter hook.
The exact task source is retained as tests/json/task_rejected.void, and the test now
checks that existing language diagnostic. Original failed assertion logs and receipts
are retained. Schema tests now include all four execution modes and 21 exclusions;
bounds cover all eight invalid settings and exact max_work 13/14/15 for null (56
vectors); adversarial tests cover 64/128 typed depth, 64 FNV-colliding keys, shared
aliases, all ten scalar adapters and decoded control/surrogate bytes (22 vectors).
Final supplementary attempts must be rerun after the assertion correction/repair.

The source module itself remains unchanged since source freeze 3. C++ generator,
test, and internal documentation changes require a later final hash inventory.
Release informational warmed O2 measurements (20 iterations, compile excluded):
save 125 bytes/12 nodes parse 71.11ms and stringify 46.16ms; config 90/9 48.54ms/
34.62ms; depth64 129/65 258.76ms/271.17ms; width128 1445/129 2736.01ms/526.67ms.
These are observations, not timing gates; work-bounded ordered duplicate checking
is deliberately quadratic. Root/allocation observations remain in the host test.

## Final source freeze and verification run

Source freeze 8 supersedes freezes 5–7 (only native-host observation/test changes
after freeze 5; the source library and production generator are unchanged). The
wide DTO repair executes with exact output in 246.61 seconds at O0. Shapes O2
executes with exact output in 148.76 seconds. Machine scheduling of large schemas
remains expensive; this is an explicit compile-latency limitation, not a runtime
parser timing gate. Fixtures are split per executable case and fixed/wide workload,
with 600-second large-schema process allowances and the same actual 1 MiB stack.
The native fault sweep has a 1500-second allowance within its existing 1800-second
CTest limit. No optimizer, stack, leak, ASan or UBSan setting was weakened.

Original consolidated selections: Debug 22/24, Release 22/24, strict 21/24. Seven
failed group attempts are retained: Debug execution/group timeout and stress
timeout; Release shapes/process timeout and stress timeout; strict execution/group
timeout, stress timeout and the corrected task-diagnostic assertion. These are
separate from effective final results and from the original six readonly incidents.
The original host passes 12,335 checks in every profile, with 2050 injected failures
per mode and zero remaining temporary objects/Runtime handles. LSP passes 43 checks.

Fresh final selections now include 31 CTests/profile (18 JSON, 13 affected shared).
Debug/Release are running against rebuilt production binaries. Strict source build
retains all sanitizer flags; its final selection follows that fresh build. The host
now prints root/construction/poll counts and additionally forces collection through
the new wide-construction closure with 32 public const managed children, both modes.
No final pass or completed HANDOFF is claimed until all receipts are reviewed.

## Verification recovery: generic inference and final GC

The expanded scalar driver nested `parse_as<t>(s)?` directly inside inferred
`stringify`. Ordinary generic inference selected the result wrapper. The frozen
pre-edit compiler reproduces this without JSON in
`tests/json/reductions/generic_nested_propagation.void` (`identity(read()?)`).
No compiler semantics were broadened. The driver now binds each payload first,
as the approved sequential user experience does. Debug/Release reruns pass all
22 vectors in all four modes; strict's final driver also passes. The failed source
diagnostics remain in original receipts. A deterministically regenerated, hash-
matching reproduction is labeled as such, not as the original captured artifact.
API/HANDOFF explicitly record this existing language limitation.

All three updated native hosts pass 12,343 checks. In each mode, the wide const
probe records 223,824,205 root observations, 266 managed constructions and 50,564
polls; the save/parse/error probe records 12,087,416 root observations, 85 managed
constructions and 11,005 polls. Each mode's sweep covers 2050 injected allocation
failures followed by successful retries. Zero temporary objects and handles remain.
The stable Debug/Release host rechecks replace their earlier pre-probe host rows;
the separate race audit explains why those earlier post-run hashes are insufficient.

For the earlier Debug/strict 900-second group timeouts, the last complete child
was shapes O0 native. The next shapes O2 command follows from driver order, rather
than a captured child receipt. Postmortem source copies are verified against the
last child's originally loaded-input hashes and retain all 28 files per profile.
The original parent JUnit/status/logs remain intact. No reconstructed source is
labeled an original captured artifact. Splitting current cases avoids that group
budget conflict. Source freeze 10 and a source/build audit verify 177 source files
and the protected root README/research archive; final wide AOT runs still remain.
