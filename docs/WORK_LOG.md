# Work log

**Current handoff:** [Documentation index](README.md), [Sprint 3 acceptance](stage8/SPRINT3_ACCEPTANCE.md) and [contract](STAGE8_TEXT_COLLECTIONS.md). Stage 8 Sprint 3 is locally verified and stopped for Wrench review; Sprints 4–6 remain future work. Hosted CI is deferred to Stage 8 end. Earlier entries retain historical evidence.

## 14 October 2026: Phase 22 UTF-8 foundation implementation

S1-S8 are owner-approved, including resource limits. Implementation and full local
verification are complete. All six matrices, stress repeats, small-stack checks,
real editor integration and VSIX packaging passed. Stop for owner review before
the unchanged seven-job hosted gate.
The accepted VOID1001 output amendment remains integrated. No future indexing,
length/byte_length, nullable strings, interpolation execution, ordering, formatting,
print or collection feature was added. Language Bible Section 8.1 records the
supplied future decisions as approved design, not implemented syntax.

### Implemented contracts

- Added one intrinsic string Type, owned ConstantValue text and a deduplicated
  IR byte-template pool. The existing frontend, typed AST, pure evaluator,
  construction/default plans and independent IR validator handle literals,
  exact content equality, +/+=, empty defaults, constants, fields, parameters and
  returns. Only two obsolete string-type deferral assertions were replaced with
  unknown-type negatives; new positive/negative string families cover the new
  behavior. No existing suite or meaningful safety assertion was removed.
- String tokens retain exact written spelling/spans and decoded bytes separately.
  Strict escapes, scalar validation and 16 MiB literal/folded-result limits apply
  before oversized append/concatenation. One shared pool interner applies the
  64 MiB unique-template limit in analysis and lowering, including owned declaration
  metadata. Pure defaults/constants remain templates with fallible materialization,
  not eager runtime module initialization. Unterminated literals recover on their
  physical line; import-path grammar remains separate.
- Nonempty strings occupy one intrinsic leaf allocation on the existing Heap.
  Internal zero denotes empty, without introducing source null/string?. Layout
  kind/extent/UTF-8 checks prevent ordinary objects or nominal descriptors from
  masquerading as intrinsic text. Runtime scratch contains complete copied bytes,
  no managed pointers, and feeds the existing construct_into transaction. Allocation
  budgets, collection/poll, last-step publication and checked failure cleanup are
  reused. No second allocator, collector, string heap or pending-object registry.
- The existing bounded logical shape walker includes strings for tracing while
  excluding them from mutable-referent authority. Mixed managed/interface readonly
  capabilities remain intact. Shared LLVM lowering roots string-only programs,
  nested values/fields, inherited layouts, actual stage/receiver storage, arguments,
  results and fixed loop slots. Existing no-GC call/return handoffs remain. Native
  private signatures and the i32 CLI adapter are unchanged.
- The same scalar UTF-8 validator now serves source, IR and native/runtime helpers
  without a frontend/LLVM runtime dependency. Embedded NUL remains length-aware
  ordinary content. native_utf8 still rejects NUL for terminated APIs. The real C
  fixture proves both terminated copies and a retained pointer-plus-length copy:
  original managed storage is collected, provider reads retain exact NUL bytes,
  provider release clears references before byte-owner teardown. This synchronous
  provider proof does not authorize arbitrary asynchronous retained-pointer APIs.
- Heap payloads/metadata contain no AST/LLVM/global/code pointer or generation
  lease. Tests observe old string-bearing holders surviving allocating JIT-code
  retirement, compatible active body reload with concurrent collection, and new
  separately linked AOT code reading the old typed holder. Const/default changes
  still require restart. No public aggregate/string ABI or hidden string cache.
- Added STRINGS.md and updated the approved proposal, Bible future-design section,
  decisions, source guide, known issues and README. LSP tests cover imported text
  constants, UTF-16 escape spans and unsaved failure/repair; grammar and real-editor
  tests cover string/type/escape regions and reserved interpolation diagnostics.

### Frozen verification inputs and preliminary failures

The final 288 source/test/editor/build inputs are recorded by SHA-256 in
build/phase22-verification/frozen-source.json. No implementation input changed
during the final matrix. Both SDKs report LLVM 19.1.7 with ORC headers, libraries
and LLVMConfig.cmake; installed RTTI YES and official archive RTTI NO remain
matched throughout the program. Existing strict sanitizer settings are unchanged.

Initial targeted string/frontend/LSP/normal-O2 AOT/CLI checks passed 6/6. The first
broader verification overlapped the final audit: an append-guard edit had a missing
parenthesis, and builds configured before the new native fixture wiring linked
without its C library. These were corrected before the freeze. The pre-freeze
logs, including failures, are retained under pre-freeze-matrix and are not final
acceptance evidence. The initial official small-stack listing still contained
only six prior suites; pre-freeze-stack is also excluded. The final stack script
asserts exactly seven requested suites, including strings, before executing them.

The final matrix uses `python3 build/phase22-verification/validate.py`: each tree
runs `cmake -S . -B build/<tree>`, `cmake --build build/<tree> -j2`, then
`ctest --test-dir build/<tree> --output-on-failure -j4`. Matrix results are recorded
below; earlier baseline/diagnostic-only
tables elsewhere in this log remain historical evidence.

| Build tree | Configure seconds | Build seconds | CTest seconds | Result |
| --- | ---: | ---: | ---: | --- |
| ci-debug | 9.34 | 2.72 | 73.45 | 59/59 |
| ci-release | 9.41 | 66.89 | 29.01 | 59/59 |
| ci-sanitizers | 9.77 | 218.77 | 113.13 | 59/59 |
| ci-official-debug | 8.97 | 86.29 | 70.59 | 59/59 |
| ci-official-release | 9.17 | 214.79 | 38.64 | 59/59 |
| ci-official-san | 9.12 | 341.55 | 85.59 | 59/59 |

All **354 full-suite executions passed**. The existing 55 suites remain, with
strings_source, strings_aot_normal, strings_aot_optimized and strings_cli bringing
the total to 59. CLI fixtures run -1/0/10 through normal/O2 JIT and independent
AOT. For input 10, main/stress/initialization/unicode return 11/310/12/13. The two
independent AOT harnesses link runtime plus the C fixture, without LLVM libraries;
they check normal/O2 roots, checked failures and new-code access to old holders.

Strict runs retained `ASAN_OPTIONS=detect_leaks=1:halt_on_error=1:exitcode=87` and
`UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1:exitcode=86`, outside the process
inspection sandbox so LeakSanitizer could operate. No leak check, sanitizer,
assertion or platform suite was disabled. The existing official-SDK no-RTTI/vptr
limitation remains unchanged. Generated application code does not gain full
ASan/UBSan instrumentation merely because the C++ host/runtime is instrumented;
independent IR, explicit roots and native bounds/publication assertions remain
necessary evidence.

Final `python3 build/phase22-verification/stack.py` used exact soft/hard
RLIMIT_STACK=1048576. It asserted seven selected suites existed and ran frontend,
managed initialization, reference values, composition, inheritance, interfaces
and strings three times per Debug SDK. All **42 executions passed**: installed
SDK 99.09 s, official SDK 90.07 s. No stack-limit assertion was weakened.

Editor tools remain Node 24.19.0 and npm 11.6.2, using existing installed tools
without dependency/lockfile updates. `npm test` passed; explicit
`node --test --test-isolation=none out/test/config.test.js` confirmed 4/4 cases.
Final real editor integration used the frozen build/ci-debug/voidls with
`VOIDLS_PATH=<repo>/build/ci-debug/voidls VSCODE_EXECUTABLE_PATH=/usr/share/code/code
npm run test:host`. Normal, restricted-trust and multi-root scenarios passed,
including real native-process kills, restart/failure/deactivation assertions and
the new unsaved string error/repair/navigation checks. VS Code ran on Linux;
VSCodium is not installed and is not claimed as a separately executed host.
`npm run package` produced the 149139-byte VSIX, and its string grammar bytes
match the source grammar exactly. Editor/unit/package logs and package hash are
under build/phase22-verification/editor/.

Repeated stress used `python3 build/phase22-verification/stress.py`, dispatched
over the six completed configurations. Each uses CTest `--repeat until-fail:3 -j2`
for exactly 17 suites: the preceding runtime/generated/callback/invariant/failure,
managed/init/reference-value/composition/inheritance/interface source and native
families plus string source and both AOT suites. The script asserts the requested
inventory exists before running. All **306 executions passed**.

| Stress tree | Result | Seconds |
| --- | --- | ---: |
| ci-debug | 17/17, three repeats | 78.82 |
| ci-release | 17/17, three repeats | 35.21 |
| ci-sanitizers | 17/17, three repeats | 76.01 |
| ci-official-debug | 17/17, three repeats | 76.17 |
| ci-official-release | 17/17, three repeats | 24.22 |
| ci-official-san | 17/17, three repeats | 52.87 |

Final input SHA-256 comparison matched all 288 frozen files. `git diff --check`
passed; the new documents contain no em dashes. Logs, SDK evidence, source hashes,
per-configuration matrix/stress JSON and stack/editor/package evidence remain in
build/phase22-verification/. The pre-freeze failures remain separate and visible.

This establishes local Linux verification, not hosted Windows/macOS acceptance.
Both complete LLVM 19.1.7 SDKs, Debug/Release, strict sanitizers/leaks, normal/O2
JIT, independent native AOT, GC/root/OOM/reload stress, 1 MiB checks, real editor
and VSIX packaging were exercised without weakening checks. No production GC,
hard real-time behavior, arbitrary native-provider lifetime, public managed ABI
or deferred string API is claimed. No workflow/toolchain/dependency update or Git
mutation was performed. Phase 22 is ready for boss/wrench-clanker review; the owner
handles Git and the seven hosted jobs. Do not advance to a new phase automatically.

## 14 October 2026: Phase 22 S1-S8 implementation authorized

Boss and wrench clanker explicitly confirm S1-S8 and their recommended resource
limits approved. Full UTF-8 string foundation implementation remains authorized;
the accepted diagnostic amendment was additional work. STRINGS_PROPOSAL.md now
records approval and implementation-in-progress status. No additional design gate
is required for the foundation. The Language Bible records the supplied approved
future scalar indexing, length/byte_length, nullable-string and interpolation
direction as not implemented and outside Phase 22. Ordering and out-of-range
behavior remain open. Final implementation and validation evidence will follow;
no completion or hosted verification is claimed at this stage.

## Phase 18 reference-containing value structs: design only, 13 October 2026

The owner declares Phase 17 locked and reports Actions run #29 passed all seven
jobs with 39 suites per configuration. This is owner-reported hosted evidence;
no remote run/repository was accessed or modified. It supersedes historical pending
Phase 17 hosted wording below. Phase 18 implementation is **not authorized**.

Reviewed the relevant Language Bible value/reference identity, readonly, defaults,
initialization, GC, reload and compiler contracts; Development Strategy and its
current-phase addenda; VALUE_SEMANTICS, VALUE_STRUCTS, INITIALIZATION,
MANAGED_STRUCTS, MANAGED_INITIALIZATION, GC_RUNTIME, shared analysis compatibility,
receiver/module guides, decisions/known issues and recent work log. Inspected the
actual nominal/typed AST, capability effects, C1 default/argument plans, S1 views,
Place selection, IR validation, LLVM layout/root/debug/call/return/failure lowering,
Heap/Frame/Slot/publication/ABI/bootstrap, callbacks/provider quiescence, leases,
analysis/invalidation/LSP and existing native/source tests. Initial worktree was clean.

### Findings and proposed contract

Created docs/REFERENCE_VALUE_STRUCTS_PROPOSAL.md, recommending inline value copies
with identity-preserving managed reference leaves, one shared access/leaf model,
existing aggregate lowering and precise Frame/Slot/TypeLayout infrastructure.

Concrete gaps, not assumed completed facilities:

- Frontend layout registration explicitly rejects reference-containing value fields.
  Removing that line alone is unsafe. Required-reference omission checks currently
  apply only to direct fields of no-init managed construction; zero-shaped default
  metadata is not a valid default for nonnullable references.
- Current capability analysis clears origins/readonly on aggregate variables and
  results, tracks only direct managed parameters/returns, and can lose authority
  through nested value copies. Current aggregate IR nominal equality/field extraction
  is not a per-reference capability verifier.
- Existing selected-target lowering and shared LLVM reference_offsets already
  traverse nested inline aggregates. Private Phase 15 IR proofs exercise aggregate
  copies, actual receiver storage, argument/return handoffs, loops and JIT/AOT GC.
  They do not prove Phase 18 source typing/readonly/construction semantics.
- One receiver_mutates flag conflates inline storage writes and referent effects.
  Copies of value parameters are independent, but their embedded references would
  still mutate original targets; summaries cannot drop that origin at a value copy.
- S1 field readiness is deliberately whole-field for complete nested values. C1
  requires valid value fields before custom value init; no partially initialized
  value-child borrowing or leaf definite-initialization contract is already approved.
- Heap descriptors/frames support numeric nested pointer offsets and completed
  references, not source readonly/nonnullable policy. LLVM currently guards direct
  nonnullable publication fields; nested required leaves need explicit review.
- Shared analysis/tooling compatibility currently compares one readonly flag and
  aggregate identity. New leaf/effect facts must be serialized/compared by the
  compiler-owned model, including result capability substitution at readonly calls.

RV1–RV7 in the proposal identify recommended choices and alternatives:

- Per-reference-leaf readonly profiles preserve mixed authority without tainting
  unrelated references. Var copies can edit independent scalars/reference slots;
  copied readonly referents remain readonly. Existing binding profiles remain fixed;
  mutable managed fields cannot receive readonly carriers and launder access.
- Reuse plain/const formals and inferred effects. Distinguish inline storage writes
  from referent mutation/retention; permit referent-only calls on suitable temporaries
  without granting mutable inline storage or adding ownership syntax.
- Preserve C1's all-fields-valid-before-value-init rule. No-init values can require
  explicitly supplied nonnullable references; custom value init uses safely
  defaultable fields (including nullable refs). S1 managed required-reference rules
  do not silently expand to unavailable nested values. Richer value readiness is
  an explicit alternative needing additional design/approval.
- Infer fixed fresh value-init receiver/result capability profiles, allowing const
  inputs to populate readonly leaves without granting mutation. Check the whole
  body under that profile, conservatively, without per-object specialization.
- Permit pure init-free all-null templates. Preserve the existing compile-time
  literal versus runtime readonly-view distinction described below. Runtime const
  copies/const formals retain their readonly profile even when currently null.
- Reuse the same nominal/IR/LLVM/GC path, with bounded inline leaf enumeration,
  independent IR capability checks, actual receiver roots and original selection/
  evaluation order. Preserve S1/P3/P4, code leases, provider quiescence, entry and
  native ABI boundaries. No production GC or aggregate FFI claim.

The proposal includes complete/fragment source examples, valid and rejected cases,
constructor/publication/failure rules, a concrete implementation sequence, risks,
resource bounds and acceptance gates for all 39 prior suites, six Linux SDK
configurations, strict sanitizers/leaks, repeated GC stress, 1 MiB stack, normal/O2
JIT, runtime-only linked AOT, LSP/editor/VSIX and all seven hosted configurations.
It explicitly requests narrow replacement of obsolete feature-deferred assertions
only after implementation approval; no assertion changed in this design phase.

### Focused unchanged-baseline verification

Used existing **Debug Phase 17 binaries**; no configure or rebuild. Verified both
SDK llvm-config version 19.1.7, RTTI YES installed / NO official, and presence of
LLJIT.h, libLLVMOrcJIT.a and LLVMConfig.cmake. Existing cache LLVM_DIR paths match
the respective installed and complete official SDKs. Actual commands:

```sh
ctest --test-dir build/ci-debug -R '^(managed_.*|frontend|runtime_generated|runtime_failures|reload|lsp_protocol|initialization_cli|analysis)$' --output-on-failure -j2
ctest --test-dir build/ci-official-debug -R '^(managed_.*|frontend|runtime_generated|runtime_failures|reload|lsp_protocol|initialization_cli|analysis)$' --output-on-failure -j2
ctest --test-dir build/ci-debug -R '^runtime_aot_.*$' --output-on-failure
ctest --test-dir build/ci-official-debug -R '^runtime_aot_.*$' --output-on-failure
```

- Broad selections: 15/15 passed for each SDK, 12.43 s installed / 12.14 s official.
- Additional private aggregate AOT selections: 2/2 passed for each, 0.12 s each.
  Total 34 selected baseline suite executions. Both normal/O2 modes are exercised
  by existing source/runtime proofs; this is not 39-suite/full-matrix verification.
- An unchanged installed voidc check of a temporary source containing
  `value struct packet` with `owner: node?` rejected at line 5 column 5 with
  VOID1103, "Reference-containing value structs are deferred." This confirms
  source support remains absent, despite private aggregate-root IR proofs.
- An existing-source probe of module `const absent: node? = null`, copied to var,
  replaced with a fresh node then mutated, returned 11 for input 10. Current pure
  constants materialize as literals; no object identity/readonly target exists
  in compile-time data. RV5 recommends retaining this for all-null templates while
  preserving runtime readonly views. A uniform readonly rule even for compile-time
  templates would narrow this locked case and needs explicit approval; it must not
  be silently inferred from new aggregate handling.

Evidence is in /tmp/void-phase18-investigation: sdk.json, installed-baseline.log,
official-baseline.log, installed-aot-baseline.log, official-aot-baseline.log,
source-boundary.log and null-module-constant.json, with temporary .void probes.
These are local temporary investigation files, not durable CI artifacts.
No new full Release/sanitizer/stress/editor or hosted run is claimed. Baseline
success is **not Phase 18 implementation proof**; future source/root/readonly and
actual Windows/macOS ARM64 acceptance remain required after approval/implementation.

Only the new proposal and this work log changed. The Language Bible, compiler,
runtime, tests, extension, LLVM requirements and seven-job workflow are untouched.
git diff --check, proposal fence/whitespace checks and final change-scope review
passed. CTest inventory remains 39. No commits, pushes, branches or remotes changed.
Stop here for architectural review and explicit owner approval of the proposal
before Phase 18 implementation.


## Phase 17 managed custom initialization: implementation, 13 October 2026

The owner approved **S1 + MI1–MI7 exactly**, including the narrow replacement of
Phase 16's obsolete valid-managed-init deferral assertion. This supersedes the
design-only stop entry below. Phase 16 is locked with owner-reported run #28,
all seven jobs and 35 suites/configuration. No GitHub/remote action was performed.

### Implemented contract and architecture

- Ordinary managed init now executes through the existing source compiler. C1
  positional/named/default argument binding, exact typing, written evaluation
  order, private-owner factories, nested reference-free value defaults and direct
  lifecycle rejection are preserved. No-init managed named-field construction,
  explicit nonnullable managed bindings and null nullable defaults are unchanged.
- The receiver is a compiler-checked staged construction view, never an incomplete
  managed reference. Parent identity appears only after body success and final
  gc_construct commit. Required direct reference fields need definite assignment
  before read/subfield/compound use and on every normal exit. Branch/loop/break/
  early-return readiness is checked. Required value children retain C1's earlier
  default-construction boundary; no implicit managed child allocation was added.
- A bounded worklist over the already nominally typed AST proves local aliases,
  safe helper/method calls, recursive contexts and multiple parameters aliasing
  one place. All fields must be ready before whole-view calls. Whole-self identity,
  nullable injection/casts, retention, constructor arguments, returns and rebinding
  reject with source diagnostics; unsafe helpers relate their prohibited use.
  Contexts are declaration/parameter masks, capped at 1024/project and reused
  recursively. A mask-explosion source test proves diagnosed exhaustion.
- P3 deep readonly remains compile-time capability: const views/formals cannot
  thaw via var/type annotations/casts/returns. Completed child aliases and
  independent reference-free value copies retain their existing semantics.
- Shared IR Type/Places carry private construction capability/origins and
  stage_begin storage. An independent must-dataflow CFG proof checks formal/owner
  storage, active scope, readiness, helper calls, once-only marked initialization,
  final field reads and commit. Malformed IR tests reject missing initialization,
  unavailable reads, omitted stage, invalid origins/formals and attempts to replace
  custom construction with legacy allocation. Existing internal allocation proofs
  remain valid; no source path bypasses initialization.
- Shared LLVM lowering uses target-aligned private payload storage, zeroed before
  frame entry and each new loop construction. Precise roots describe actual mutable
  staged field storage, including helper formals; staging pointers are never heap
  roots. Existing expression/argument/return roots, O2 polls, failure cleanup and
  module-owned DataLayout/debug fixes remain. Existing Runtime::construct_into,
  Heap, ABI/bootstrap and hot-reload leases are reused without runtime changes.
- Shared analyzed declarations expose construction masks and required field names,
  retaining existing initializer/default spans and references. Compatibility and
  tooling invalidation compare these facts. LSP constructor/helper navigation uses
  original source identities; unsaved readiness errors invalidate stale queries,
  and repair restores them. C1 conservative init/helper closure reload policy is
  preserved; an unrelated compatible body can still reload with the same runtime.

Added examples/managed_initialization and four suites: managed_initialization_source,
managed_initialization_aot_normal, managed_initialization_aot_optimized and
managed_initialization_cli. Inventory is **39**; all 35 prior suites remain. Only
one obsolete feature-deferral assertion was replaced, with positive constructor
execution. Existing direct-init, readonly, lifecycle, failure, native, GC, reload,
small-stack and editor assertions remain. CMake factors the existing managed source
AOT/CLI wiring into one reusable family function, preserving Phase 16 target names,
runtime-only native links and Windows legacy_stdio_definitions dependency.

The test-only ABI probe now optionally inspects actual Frame/Slot leaf storage at
entry/leave/poll/commit, forwarding to the real ABI. It checks every nonnull leaf
belongs to the actual heap, not staging bytes. Source stress observes 303 completed
constructions and counter 213 before parent publication; budget-1 and forced GC
are independent checks. Captured complete parent/child graphs survive analysis/code
retirement then reclaim after handle release. No synthetic source IR or replacement
collector/allocator was used. Completed child temporaries/returns survive recursive
helpers, loops and O2. Body/default/argument/child failures, injected final commit
OOM/bookkeeping failures and bootstrap failures preserve publication/output and
balance roots; prior body effects remain, as approved.

Deterministic source safepoint tests gate a worker after completed child allocation,
request a real collection while the parent still has no identity, then release it
into the actual poll. A separate final-commit gate admits a noncooperating Managed
participant after body collections; the source's budget-1 allocation joins the
collector and parks. The deliberate one-second rendezvous timeout must release
both participants, report allocation failure, leave object count unchanged (no
sweep/parent publication), unlink frames and recover on a subsequent invocation.
No sleeps or iteration inflation establish overlap; cooperative park and epoch/
request outcomes are asserted.

### Corrections during focused verification

An early build required the standard algorithm include for std::none_of. Initial
new-harness checks exposed handle reads outside an Attachment/Managed scope and
an overbroad caller-frame expectation for argument failure raised directly in entry.
Those fixture issues were corrected without runtime/compiler changes or weakening
meaningful source/frame assertions. The first deliberate-timeout fixture used the
default source allocation budget, so its final commit was not a collecting caller;
setting the explicit test runtime to budget 1 proves the intended joined-collection
failure path. A normal poll alone is not asserted to propagate collector failure.
Focused suites passed before full validation. Final independent-IR review added
legacy-allocation and missing-formal-storage guards plus negative regressions; the
**entire six-configuration matrix was rerun on those final sources**.

### Final full Linux validation

Fresh configure/build/test completed with **39/39 in every configuration**, 234
suite executions in this final matrix:

| LLVM 19.1.7 SDK | Debug | Release | RelWithDebInfo ASan/UBSan |
| --- | --- | --- | --- |
| Installed, RTTI ON | 39/39, 23.04 s | 39/39, 17.44 s | 39/39, 35.71 s |
| Complete official Linux archive, RTTI OFF | 39/39, 18.50 s | 39/39, 13.75 s | 39/39, 34.65 s |

Both SDKs were verified for exact llvm-config version 19.1.7, headers,
libLLVMOrcJIT.a, LLVMConfig.cmake and RTTI. Every configure executes the real ORC
SDK link/execution probe. Matching toolchains and the existing official no-RTTI ABI
correction are intact; installed vptr instrumentation, ASan/UBSan error halting and
leak detection remain. No sanitizer suppression or disabled assertion/test was added.

For ci-debug, ci-release, ci-sanitizers, ci-official-debug, ci-official-release and
ci-official-san, the actual commands were:

```sh
cmake -S . -B build/<tree>
cmake --build build/<tree> -j2
ASAN_OPTIONS=detect_leaks=1:halt_on_error=1:exitcode=87 \
UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1:exitcode=86 \
ctest --test-dir build/<tree> --output-on-failure -j4
```

Evidence: build/phase17-verification/validate.py, sdk.json, results.json, matrix.log
and individual configure/build/test logs. initial-results.json and initial-matrix.log
retain the earlier six green runs before final IR-proof refinements; final results
above supersede those timings. Strict sanitizer execution ran outside the tracing
sandbox so LeakSanitizer can perform its normal check.

Additional final-source verification:

- Both Debug SDKs: ctest -R '^(runtime_.*|managed_.*)$' --repeat until-fail:10
  --output-on-failure -j4 passed all 15 selected suites ten times each
  (50.08/42.97 s installed/official).
- Both Debug SDKs: ulimit -s 1024; ctest -R
  '^(frontend|managed_source|managed_initialization_source)$'
  --repeat until-fail:5 --output-on-failure passed all three suites five times each
  (69.40/59.25 s). Includes the unchanged bounded-layout semantic regression and
  the new context-exhaustion and source construction proofs at a real 1 MiB limit.
- Both strict sanitizer SDKs: ctest -R
  '^(runtime_.*|managed_.*|frontend|reload|gc|native)$'
  --repeat until-fail:3 --output-on-failure -j4 passed all 19 selected suites three
  times each (37.65/28.93 s), including leaks and old architectural assertions.
- Both Release SDKs: ctest -R '^(managed_source|managed_initialization_source)$'
  --repeat until-fail:30 --output-on-failure passed both suites all 30 times
  (154.58/143.77 s). These run normal/O2 JIT, deterministic overlap/timeout, roots,
  failure injection and reload on every execution. extra.py, extra-results.json
  and individual logs retain all 564 additional repeated suite executions.
- Existing Release coordinator fixtures passed both SDKs. NativeSafe baseline
  (1000 collections, one frozen object) mean/max: installed 0.129469/14.78 us,
  official 0.168578/1.61 us. These fixture values are not production pause claims.
- Runtime-only TSan: clang++-19 -std=c++20 -g -O1 -fsanitize=thread
  -fno-omit-frame-pointer -Iinclude tests/runtime/coordination.cpp
  src/runtime/coordination.cpp src/runtime/heap.cpp -pthread; run with
  TSAN_OPTIONS=halt_on_error=1:exitcode=85 passed with no race report. This does
  not instrument prebuilt LLVM/generated instructions or claim full JIT TSan.
- Extension npm test passed; an explicit Node 24.19.0 spec-reporter run confirms
  all four individual unit tests. npm run package passed, producing the eight-file
  145.44 KB VSIX. Actual installed VS Code 1.141.0 integration passed trusted,
  restricted and multi-root modes, native unsaved diagnostics/navigation, real
  process termination/notification counts, restart and deactivation cleanup.
  A final host run after the final native rebuild also passed. editor-unit.log,
  editor-unit-detail.log, editor-package.log and editor-host-final.log retain evidence.
  No extension files changed. VSCodium is unavailable locally; no VSCodium run is claimed.

### Actual native source demonstration and delivery

```sh
build/ci-debug/voidc run examples/managed_initialization/main.void 10 --no-opt # 26
build/ci-debug/voidc run examples/managed_initialization/main.void 10          # 26
build/ci-debug/voidc emit examples/managed_initialization/main.void -o build/phase17-verification/managed-init.o
clang++-19 build/phase17-verification/managed-init.o build/ci-debug/libvoid_runtime.a -pthread \
  --ld-path=/usr/lib/llvm-19/bin/ld.lld -o build/phase17-verification/managed-init
build/phase17-verification/managed-init 10                                  # 26
build/ci-debug/voidc run examples/managed_initialization/stress.void 10       # 246
```

file identifies managed-init as a real x86-64 ELF executable with debug info;
ldd shows only system C/C++ libraries, no LLVM dependency. This is actual parsed
source emitted to a native object and independently linked, using the same shared
lowering as ORC JIT. CLI suite separately executes eight fixtures × three inputs ×
normal/O2 × JIT/AOT (96 executions) per configuration, including accurate failure
traces without synthetic construction variant names. Additional local check:
imported private managed init diagnoses VOID3003 at the constructor token, while
an omitted numeric parameter default plus an independent value-field copy through
a const staging alias returns 16 under normal/O2 JIT (visibility.log/defaults-results.json).

Updated MANAGED_INITIALIZATION.md, approved proposal status/implementation record,
INITIALIZATION, MANAGED_STRUCTS, VALUE_STRUCTS, GC_RUNTIME, shared analysis compatibility, source
compiler, DECISIONS, KNOWN_ISSUES, Development Strategy, README and focused Bible
clarification of the owner-approved managed contract. No broader language policy
was finalized. Self-identity/self-links/escape in init, borrowed returns/rebinding,
helper-assisted first initialization, reference-containing values, runtime defaults,
fallible results/resources/deinit and unrelated features remain deferred. Next
recommend designing reference-containing value capabilities/copy/readonly/root rules.

The implementation is ready for owner review and hosted CI. All seven jobs must
validate the 39-suite inventory after the owner pushes, particularly Windows
MSVC/SDK linking and macOS ARM64 target layout/debug/O2 behavior, plus Debug editor
runs on each platform. Local execution is Linux x86-64 only; hosted success is not
claimed. The seven-job workflow, LLVM 19.1.7, runtime/GC implementation and strict
settings are unchanged. No commits, pushes, branches or remote modifications.
Final git diff --check and explicit whitespace checks of all new files passed.
A parsed comparison with the Phase 16 CTest log confirms all 35 original names
remain and exactly the four Phase 17 names were added. .github/workflows/ci.yml,
src/runtime and editors/vscode have no diff.


## Phase 17 managed custom initialization: design only, 13 October 2026

The owner declares Phase 16 locked and reports Actions run #28 passed all seven
jobs with 35 suites per configuration. This is owner-reported hosted evidence;
no GitHub action was performed in this session. It supersedes older Phase 16
pending-hosted wording below and in historical scope documents. No implementation
of Phase 17 is authorized or included.

Read the relevant Language Bible safe-default/identity/init rules, current
initialization/managed/runtime guides and proposals, Development Strategy,
DECISIONS, KNOWN_ISSUES and Work Log. Inspected the actual nominal/typed AST,
constructor binding/default planning, readonly/effect analysis, Places/shared IR
validation, LLVM root/layout/call/failure lowering, Heap/construct_into/ABI/bootstrap,
reload dependencies, native/callback contracts and existing source/AOT tests.

Key findings:

- Managed init is deliberately rejected in validate_graph. Managed lowering stages
  field operands then gc_construct; only the value branch invokes init. Removing
  the diagnostic would not create a safe managed initializer path.
- Value init aliases aggregate storage; ordinary managed receiver ABI/root slots
  represent heap references. Passing staging bytes through that ABI or changing a
  managed nominal to a source aggregate defeats actual IR/root invariants.
- Runtime::construct_into already provides a reusable final commit: collect/poll
  before the lock, complete fallible Heap work, copy prepared bytes, root publication
  last. It must not execute initializer code under its lock or accept a partial
  parent reference. The existing runtime has no pending-object identity registry.
- Existing readonly retained/returned origins flow through child fields. They
  preserve deep readonly but cannot alone distinguish escaping a staged parent
  from returning a completed child. Construction eligibility also needs identity,
  rebinding, whole-view escape and passing-category facts in the same semantic model.
- Safe defaults exist for scalar/value/nullable fields. Non-nullable managed fields
  have no implicit allocation/default. C1's all-fields-defaultable rule cannot simply
  be copied to useful managed init with required reference fields.
- A private root containing a real unfinished parent still violates literal P4.
  Calling a default-filled parent complete before init or suppressing GC is not a fix.

Created MANAGED_INITIALIZATION_PROPOSAL.md recommending S1: compiler-only staged
construction views, precise roots for completed child references, inferred safe
helper/method contexts, then existing complete-object commit. No parent managed
identity exists during the body. The proposal evaluates privately rooted identity
(which requires explicit P4 revision) and symbolic self-links/graph commit (larger
work), plus unsafe shortcut alternatives.

MI1–MI7 identify the approval package. Major compromises: whole-self identity,
self-links/escape, borrowed returns and construction-view rebinding are unavailable
in this slice; helper calls need full field readiness; non-nullable fields may be
privately unavailable until their first proven assignment, with all normal exits
checked. This is an explicit proposed managed-only rule, not a silent C1 change.
Parameter-only binding/order/default rules extend C1; explicit managed construction,
P3 readonly, nested value construction and existing failure/side-effect semantics
remain. Final allocation follows the body; prior effects are not transactional.

The document specifies shared storage/passing categories, independently verified
IR lifecycle/readiness, current-field root mapping, finite private helper contexts
through one lowering pipeline, failure cleanup, native boundaries, code leases,
reload/tooling contracts, implementation order and seven-platform-job acceptance
gates. It explicitly requests eventual replacement only of the obsolete managed-init
feature-deferred assertion with positive execution; no test was changed now.

Focused baseline verification (unchanged existing binaries, no configure/build):

    ctest --test-dir build/ci-debug -R '^(managed_.*|frontend|runtime_generated|runtime_failures|reload|lsp_protocol|initialization_cli)$' --output-on-failure -j2
    ctest --test-dir build/ci-official-debug -R '^(managed_.*|frontend|runtime_generated|runtime_failures|reload|lsp_protocol|initialization_cli)$' --output-on-failure -j2

Both passed 10/10 selected suites: installed 9.14 s, official 8.32 s. This includes
normal/O2 parsed managed source/private linked AOT/CLI, value initializer CLI/frontend,
generated runtime roots, deterministic allocation failures, reload and LSP.
Both SDK llvm-config executables still report 19.1.7. CTest inventory remains 35.
These are focused Phase 16 baseline checks, not verification of unimplemented S1;
no new full Release/sanitizer/editor or hosted run is claimed for design-only work.

Only this Work Log and the new proposal changed. Compiler/runtime/LLVM configuration,
tests, Language Bible and the seven-runner/90-minute CI workflow remain untouched.
git diff --check passed. No commits, pushes, branches or remote modifications.
Stop here for architectural review/approval before any Phase 17 implementation.

## Phase 16 managed source implementation, 12 October 2026

The owner approved the recommended P1–P7 scope, explicitly P3 deep readonly views
and P4 reusable complete-object publication. Phase 15 remains locked; owner-reported
run #27 passed all seven jobs with 31 suites per configuration. This entry supersedes
the investigation-only status below. No hosted Phase 16 execution is claimed.

### Implemented contract and architecture

- Ordinary managed structs now compile from real source: all ten numeric/bool
  fields, reference-free nested values, nullable/non-nullable direct references,
  cyclic nominal graphs, named-field construction, identity comparisons, alias
  assignment/parameters/returns and owner-module receiver methods/imports.
- Nullable `t?`, contextual null, safe injection and checked same-nominal `as t`
  unwrap use existing diagnostics/failure propagation. Non-nullable fields require
  explicit valid values. Numeric/bool/pure field defaults, nullable null defaults
  and existing nested value init plans remain intact. Managed custom init and
  reference-containing source values remain deferred.
- Deep readonly capabilities follow aliases, casts, returned aliases, parameters
  and nested references. Existing const formals express readonly parameters;
  receiver mutation/retention/returned-origin effects are inferred on the typed
  AST. Independent mutable aliases remain valid; reference-free values copy
  independently. Existing mutable bindings reject readonly assignment; inferred
  readonly bindings retain their capability on rebinding. Monotone joins are
  deliberately conservative, not path-sensitive ownership inference.
- One nominal type table owns descriptors; weak field edges permit recursive
  reference metadata without C++ ownership cycles. Retained query Types preserve
  table lifetime. Reference graph validation is iterative; bounded inline value
  layout expansion and the small-stack protection remain in place.
- Shared gc_construct stages complete rooted fields. Shared LLVM lowering prepares
  target-layout bytes and reference offsets. Runtime::construct_into validates the
  output root/references, polls/collects before acquiring the commit lock, completes
  fallible Heap work, copies the full payload and publishes the output root last.
  No incomplete parent reaches a GC root or user code. Old outputs survive failure.
- The existing Heap, coordinator, frames, native/callback contracts, checked failure
  cleanup and hot-reload leases are reused. Source runtimes use a 256-allocation
  trigger; tests use budgets 1/8. A failed rendezvous never sweeps. Source replacement
  reuses its runtime; objects/descriptors can outlive retired code independently.
- Heap receiver/assignment targets are rooted snapshots selected before arguments/
  RHS. Reference receiver rebinding is local; nested inline receiver mutation keeps
  value-place semantics and unrelated RHS changes.
- JIT owns a runtime for managed source. Separately emitted source executables use
  the runtime bootstrap with the unchanged entry(i32)->i32 interface, linking
  void_runtime and platform C++/thread dependencies. Windows CLI targets retain
  legacy_stdio_definitions for real printf linkage. No public managed C ABI added.

The analysis/tooling model includes managed declarations/fields, nullable types,
capabilities/effects and corresponding invalidation/reload dependencies. The real
LSP protocol checks imported fields, self-types, methods, readonly edit diagnostics,
stale queries and repair. Editor presentation adds null and managed indentation;
colors/lifecycle are unchanged. Its obsolete assertion that ordinary struct should
not indent was replaced by the now-correct positive assertion, with additional
null/nullify and structure-boundary checks. Original native assertions remain intact.

### Real source and regression evidence

Added examples/managed_structs/{main,handoffs,order,imports,types,numeric,null_failure}.void.
Main builds a cycle, mutates through an alias, calls functions and allocates 1000
objects. Normal/O2 JIT and actual linked AOT all returned 23 for input 10. Independently
linked handoffs/order/numeric/imports returned 48/221/600/12. These results come
from parsing/lowering source, not reconstructed proof IR. Null unwrap fails with
its real source location and caller context in both execution modes.

Four additional suites extend 31 to 35: managed_source, managed_aot_normal,
managed_aot_optimized and managed_cli. Source tests separately establish automatic
budget-triggered collection and forced collections; complete payload observation;
cyclic reachability/reclamation; selected-receiver temporaries across allocations;
argument/return handoffs; readonly laundering diagnostics; concurrency; retained
metadata; live-object compatible reload and wrong-runtime rejection. Deterministic
poll gates require actual overlap/parking in optimized numeric loops. Allocation
failure injection exercises allocation/map/collection failure cleanup, protected
outputs and recovery. Separately linked parsed-source AOT also injects failure,
checks zero success output, unchanged live objects/frame cleanup and recovery.
AOT bootstrap tests also inject the first startup allocation failure, verify no
success output, then enter the actual compiled source again through fresh balanced
attachment/shutdown. Readonly const-parameter retention into mutable fields has
its own invalid-source assertion.
The probe forwards to the production ABI/Heap rather than replacing allocation/GC.

Two defects found during complete validation were corrected before the final run:

1. Recognizing '?' for nullable types changed the existing unsupported numeric as?
   error to an unsupported-type code. The parser now explicitly reports its prior
   character diagnostic with a checked-as hint. The old numeric assertion is intact.
2. The new test harness initially replaced ordinary new/delete but not nothrow new.
   ASan correctly reported allocation-family mismatch during LLVM SROA/machine
   sinking (installed/official SDKs). The fixture now consistently replaces ordinary,
   array and nothrow allocation/deallocation pairs. No sanitizer option/suppression,
   LLVM requirement, compiler optimization or production allocation was relaxed.

Pre-correction evidence is retained in initial-*-test.log, initial-*-build.log and
pre-probe-fix-matrix.log under build/phase16-verification. It is not final pass evidence.

### Complete stable-tree Linux matrix

Both complete LLVM SDKs verified 19.1.7, LLJIT headers, libLLVMOrcJIT.a and
LLVMConfig.cmake. Installed /usr/lib/llvm-19 reports RTTI YES; the complete official
.toolchains/ci-investigation/LLVM-19.1.7-Linux-X64 archive reports RTTI NO. CMake
continues matching each ABI throughout; strict ASan/UBSan and RTTI-compatible vptr
coverage remain as documented in CI_SANITIZER_ABI.md.

For each existing tree: cmake -S . -B build/<tree>; cmake --build build/<tree> -j2;
ctest --test-dir build/<tree> --output-on-failure -j4. Debug/Release and instrumented
RelWithDebInfo use the existing SDK/compiler/linker/cache settings. Final results:

| Build tree | CTest result | Test elapsed |
| --- | --- | --- |
| `ci-debug` | 35/35 | 23.08 s |
| `ci-release` | 35/35 | 19.98 s |
| `ci-sanitizers` | 35/35 | 37.17 s |
| `ci-official-debug` | 35/35 | 19.86 s |
| `ci-official-release` | 35/35 | 15.24 s |
| `ci-official-san` | 35/35 | 29.61 s |

ASAN_OPTIONS=detect_leaks=1:halt_on_error=1:exitcode=87 and
UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1:exitcode=86 remained enabled in
both sanitizer configurations. No tests skipped or disabled. These checks ran
outside the tracing sandbox so LeakSanitizer could inspect processes. Full configure,
build and test commands/status/timings are in build/phase16-verification/validate.py,
results.json, sdk.json and per-tree logs. The unchanged seven-job workflow runs
unfiltered CTest and will discover all 35 suites; LLVM pin, assertions, permissions,
timeout and editor Debug jobs are unchanged.

### Repetition, stack, editor and final checks

- Both Debug SDKs: ctest -R '^(runtime_.*|managed_.*)$' --repeat until-fail:10
  --output-on-failure -j4 passed all 11 selected suites ten times per SDK
  (26.96/23.71 s installed/official).
- Both Debug SDKs: ulimit -s 1024; ctest -R '^(frontend|managed_source)$'
  --repeat until-fail:5 --output-on-failure passed both suites five times per SDK
  (36.96/32.65 s). This includes the unchanged bounded-layout regression and the
  new recursive managed metadata/source tests at the actual 1 MiB stack limit.
- Both strict sanitizer SDKs: ctest -R
  '^(runtime_.*|managed_.*|frontend|reload|gc|native)$' --repeat until-fail:3
  --output-on-failure -j4 passed all 15 selected suites three times per SDK
  (25.11/24.72 s), including leak detection and all original runtime regressions.
- Both Release SDKs: managed_source repeated until-fail:30 passed every execution
  (39.41/35.52 s), including optimized generated roots and deterministic overlap.
- Runtime-only TSan: clang++-19 -std=c++20 -g -O1 -fsanitize=thread
  -fno-omit-frame-pointer -Iinclude tests/runtime/coordination.cpp
  src/runtime/coordination.cpp src/runtime/heap.cpp -pthread; executable ran with
  TSAN_OPTIONS=halt_on_error=1:exitcode=85 and passed, no races reported. This does
  not instrument prebuilt LLVM/generated instructions or claim a full JIT TSan run.
- Existing Release coordinator diagnostic passed for both SDKs (1000 collections,
  one already-NativeSafe frozen object): mean/max 0.204379/1.53 us installed and
  0.160529/1.05 us official. These fixture values are not general pause guarantees.
- Extension npm test: 4/4 cases passed; npm run package produced the eight-file
  145.44 KB VSIX. Existing local Node runtime was used; no dependency changes.
- Real installed Linux VS Code 1.141 integration passed trusted, restricted and
  multi-root modes against the rebuilt voidls. Both actual child-process kill
  scenarios retained strict notification assertions, along with unsaved diagnostics,
  navigation, repair, restart/configuration and clean deactivation. Expected
  injected lifecycle errors appear in successful logs. No VSCodium execution claim.
- ldd on the separately linked managed AOT harness showed only normal C++/C/math/
  loader dependencies, no LLVM/ORC dependency. CLI native programs also use the
  runtime-only link. An additional source check executed nullable module constants,
  a declared null field default and constant reference equality successfully.
- git diff --check passed; .github/workflows/ci.yml is unchanged. No commits,
  branches, pushes, remote modifications or test suppression occurred.

Exact repetitions/statuses are in build/phase16-verification/extra.py,
extra-results.json and per-check logs; editor-unit.log, editor-package.log,
editor-host-final.log and runtime-tsan.log record their respective checks.
Native editor/leak/TSan checks ran outside the tracing sandbox as required.

### Remaining limitations and hosted gate

MANAGED_STRUCTS.md, the approved proposal, DECISIONS, KNOWN_ISSUES, SOURCE_COMPILER,
related runtime/value/initialization guides, the Bible's approved clarification and
README now distinguish implemented managed source from deferred work. The readonly
analysis is conservative; rich managed debugger inspection, managed custom init,
reference-containing source value structs, generalized nullable patterns, FFI and
production collector policy remain future work. No GC production-readiness claim.

Phase 16 is ready for owner review and a hosted CI attempt, not locked. The owner
must push to verify all seven jobs with 35 suites each, particularly Windows runtime/
CRT linkage and macOS ARM64 object layout, roots and Debug/Release execution, plus
all three platform Debug editor/package integrations. Linux cannot certify those
platforms. No new hosted run or remote action was performed here. Recommended next
design work is managed custom init and its user-code publication/failure boundary,
then reference-containing value capabilities; retain the same shared pipeline.

## Phase 16 managed structs: investigation and proposal only, 12 October 2026

The owner declares Phase 15 locked and reports seven-runner hosted run #27 green,
31 suites per configuration. That is owner-reported hosted evidence; this design
session did not rerun or modify GitHub. Older GC/known-issue documents contain
historical pending-hosted wording and must not be mistaken for current source
feature support or a reason to reopen Phase 15.

Read the Language Bible, Development Strategy, GC_RUNTIME, INITIALIZATION,
VALUE_STRUCTS, relevant decisions/known issues and existing implementation/tests.
Traced the shared nominal/frontend/constructor/receiver pipeline, IR validator,
LLVM root/layout/allocation/failure lowering, Heap/coordinator/ABI, source CLI,
NativeCode/reload ownership, AOT adapter, analysis/tooling and runtime proof fixtures.
Created `MANAGED_STRUCTS_PROPOSAL.md`; no Phase 16 behavior was implemented.

Concrete findings:

- Source parser, fields and receivers still admit value structs only. Private
  `managed_ref` IR and nested roots do not constitute managed source compilation.
- IR validation rejects recursive/empty managed descriptors. Simply permitting
  cycles in current strongly owning Type/field descriptors would create C++
  ownership leaks. Reference type-graph validation and inline layout recursion
  must be separated, with owning immutable tables and nonowning nominal edges.
- Receiver Places describe local aggregate paths; heap projections and reference
  receivers need shared representation updates. Existing receiver-slot mutation
  effects do not account for aliasing through copied references/helper calls.
- Phase 15 allocation deposits a zeroed object into a precise root before field
  writes. It is a valid private architectural mechanism, not proof of the stronger
  complete-publication boundary requested for source constructors.
- `allocate_into` polls but does not initiate automatic collection. Heap cycles
  are reclaimed in existing tests, but source allocation-triggered reachability
  and cyclic nominal source fields have not been implemented.
- Source JIT compile/run supplies no runtime owner. Its replacement preflight is
  runtime-free. Public source AOT rejects coordinated IR; only the private native
  harness establishes the required runtime/attachment/Managed bootstrap.
- Runtime numeric descriptor copies and code leases already have separate lifetimes;
  preserve this. Source contract/tooling records need the new nominal, nullable,
  access/effect and reference-dependency metadata, with cyclic-safe ownership.

Recommended owner-review boundary: no-init managed named-field construction,
nullable/non-nullable direct reference graphs, reference identity/alias semantics,
owner-module methods, numeric/bool/reference-free nested value fields, atomic
complete-object allocation and automatic collection through the shared runtime.
Defer managed custom init and reference-containing source value structs rather than
silently permitting self escape or weakening readonly semantics. Existing custom
value init stays supported and runs once in nested value construction. The proposal
includes alternatives if the owner prefers the larger scope.

Approval requests P1-P7 cover the slice, nullable checked unwrap/injection, deep
readonly view propagation, explicit non-null initialization and publication,
automatic GC policy, unchanged-entry runtime bootstrap/reload ownership and managed
receiver/target evaluation. Deep readonly alias transfer and custom managed self
publication are genuine language decisions, not approved by Phase 14 C1 or Phase 15.
The proposal explicitly distinguishes private construction roots from published
source values; its recommended allocation path never roots an incomplete parent.

Focused baseline verification using existing binaries, without source/build changes:

```sh
ctest --test-dir build/ci-debug \
  -R '^(runtime_.*|frontend|gc|reload)$' --output-on-failure -j2
ctest --test-dir build/ci-official-debug \
  -R '^(runtime_.*|frontend|gc|reload)$' --output-on-failure -j2
build/ci-debug/voidc check examples/initialization/main.void
build/ci-debug/voidc run examples/initialization/main.void 10 --no-opt
build/ci-debug/voidc run examples/initialization/main.void 10
```

Installed SDK Debug passed **10/10, 5.89s**; complete official SDK Debug passed
**10/10, 5.57s**. These include all seven runtime suites plus frontend, GC and
reload, including real normal/O2 JIT and separately linked private AOT proofs.
The existing initialization source checked successfully and returned **166** in
both normal and O2 JIT. llvm-config verified **19.1.7 / RTTI YES** installed and
**19.1.7 / RTTI NO** official. CTest JSON discovery found **31 suites** in each tree.
This was a focused baseline, not a fresh six-configuration rebuild, sanitizer run,
Phase 16 source execution or local Windows/macOS verification.

The proposal specifies future full six-configuration local/strict-sanitizer gates,
small-stack/alias/constructor/GC failure stress, source JIT/real linked AOT reachability,
LSP/editor/reload tests and all seven hosted jobs. No new source feature, custom
initializer, runtime hook, test, workflow, LLVM setting or executable-entry policy
was implemented. Only the proposal and this work log changed. Owner approval is
required before implementation; no commits, pushes, branches or remote changes.


## CI architecture: seven parallel configurations, 12 October 2026

Phase 15 remains locked. Inspected the existing workflow and retrieved the actual
[run #26](https://github.com/Shmellyorc/Void.Language/actions/runs/38331602313)
metadata, job timestamps and all three job logs through read-only GitHub tools.
Commit `7813040149acb42a9c1b7873935587db13a631d7`, attempt 1, passed completely.
The logs confirm 31/31 for all seven native configurations, strict Ubuntu
sanitizers, the Linux small-stack regression, real editor integration in all three
workspace modes on each OS, and VSIX packaging on each OS.

### Original timing baseline

Run start: `2026-10-13T01:19:47Z`; final job completion:
`2026-10-13T02:33:18Z`. Overall elapsed time was **73m31s**, including initial
scheduling. Job durations below use actual job start/completion; configuration
columns measure configure through test-step completion, excluding platform setup,
editor work, artifact upload and the separate Linux small-stack step.

| Platform job | Job elapsed | Debug configure/build/tests | Release configure/build/tests | Sanitizers configure/build/tests |
| --- | --- | --- | --- | --- |
| Ubuntu 24.04, job 115082343730 | 73m29s | 23m06s | 23m16s | 25m23s |
| Windows 2022, job 115082343660 | 5m28s | 1m50s | 1m41s | — |
| macOS ARM64, job 115082343490 | 41m20s | 20m50s | 17m48s | — |

Raw run/job timing metadata is retained locally in
`build/ci-seven-runners-run26.json`. These are observed hosted timings, not
estimates of the new workflow's performance.

### Workflow change

Replaced the three OS entries and sequential per-configuration blocks with one
explicit seven-entry `include` matrix and shared configure/build/CTest steps:
Ubuntu Debug, Release and Sanitizers; Windows Debug and Release; macOS ARM64
Debug and Release. Each matrix expansion is its own hosted job with a readable
platform/configuration name. There are no `needs`, `max-parallel` or concurrency
restrictions introduced. All seven are eligible to run concurrently, subject to
GitHub account capacity and runner availability. Every job retains 90 minutes and
`fail-fast: false`; push, pull request, manual triggers and read-only permissions
are unchanged.

Complete official LLVM 19.1.7 installation, verification, SDK cache keys, Linux
development packages, Windows SDK environment and macOS SDKROOT setup are retained.
A macOS-only architecture assertion verifies ARM64 while preserving the existing
`macos-latest` runner label. Normal jobs use their original Debug/Release build
types; Sanitizers uses RelWithDebInfo plus `VOID_SANITIZERS=ON` on Ubuntu only.
The normal jobs explicitly pass OFF, matching the existing default. Ninja,
build parallelism 3 and LLVM optimization settings are unchanged.

Strict ASan/UBSan environment settings now live at job scope so they cover both
build-time generated-code helpers and CTest in the sanitizer runner:
`ASAN_OPTIONS=detect_leaks=1:halt_on_error=1:exitcode=87` and
`UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1:exitcode=86`. They do not enable
instrumentation on ordinary jobs. Full CTest execution is unconditional for all
seven configurations: **217 required suite executions**. Ubuntu Debug additionally
runs the existing frontend test with a 1 MiB stack.

Each platform's Debug job retains Node setup, npm ci, extension tests, real native
editor integration and VSIX packaging/upload. The existing editor harness and
VS Code/VSCodium-compatible extension are unchanged; this does not add or claim
separate hosted VSCodium binary coverage. `VOIDLS_PATH` points to the Debug build
on the same runner, preserving `.exe` on Windows and Xvfb on Linux. Release and
Sanitizers jobs do not run editor-only steps. Every test artifact includes OS,
architecture and configuration; Debug VSIX artifact names also include Debug.
Seven test artifacts and three editor artifacts have unique names. Failure-time
test/configuration log uploads retain `always()` and their original paths.
The shared SDK cache deliberately remains identical across configurations because
its complete SDK contents are identical; no compiled build outputs are shared.

### Local validation and remaining hosted gate

- Official `actionlint` 1.7.7 checked `.github/workflows/ci.yml` successfully,
  including GitHub Actions expressions and matrix contexts.
- Parsed YAML and expanded every matrix row/step condition with the local
  `build/ci-seven-runners/validate.py` checker. Confirmed exactly the requested
  combinations, unique readable names, independent scheduling, all seven full
  unfiltered CTest commands, correct build types/sanitizer flags, strict environment
  for build and tests, one small-stack job, exactly three editor/packaging jobs,
  correct native server paths and ten noncolliding artifact names. Trigger,
  permission, timeout, fail-fast and unchanged SDK setup comparisons against HEAD
  passed. `expanded-validation.json` preserves expanded commands/artifact names.
- Local CTest discovery found the unchanged 31-suite inventory. No native rebuild
  or test-source modification was needed for this workflow-only restructuring.
- `git diff --check` passed; only `.github/workflows/ci.yml` and this log changed.

The owner must push and verify all seven hosted jobs, each with 31/31, Linux strict
sanitizers, the Ubuntu Debug small-stack check and all three Debug editor/packages.
Compare the next run's start-to-final-job-completion elapsed time with **73m31s**
from run #26, and record per-job duration plus queue/cache differences. Local static
validation does not establish seven-runner scheduling or a wall-clock improvement.
Retain the 90-minute timeout until those hosted measurements justify reconsidering
it. No compiler/runtime/GC changes, test changes, commits, branches, pushes or
remote modifications were made.


## Phase 15 run #25 CI timeout adjustment, 12 October 2026

Owner-reported hosted evidence: run #25 passed Windows and macOS completely.
Ubuntu passed Debug and Release, 31/31 suites each, but was cancelled during
sanitizer linking when the build job reached its 60-minute timeout. This leaves
that run's Ubuntu sanitizer execution and subsequent steps unverified; it is
not evidence of a sanitizer test failure.

Changed only the build job's `timeout-minutes` from 60 to 90 in
`.github/workflows/ci.yml`. The platform matrix, LLVM 19.1.7 pin, build commands,
tests, assertions, strict ASan/UBSan options and leak detection remain unchanged.
Validated that the workflow differs from HEAD by exactly this one replacement,
checked the parsed job timeout, and ran `git diff --check`. No compiler/runtime
changes or native rebuilds were needed for this CI-only adjustment. The next
hosted run must confirm Ubuntu completes sanitizers and the remaining steps
within the extended timeout. No commits, pushes or remote changes were made.

## Phase 12 run #17 Windows frontend crash correction, 12 October 2026

Read the actual [Windows job 114735091143](https://github.com/Shmellyorc/Void.Language/actions/runs/38226468142/job/114735091143)
at `f9d9f3bf68828c9e46671e157cb165fa46407651` and downloaded its
`tests-Windows-X64` artifact, ID `11736648542` (ZIP SHA256
`d472dfed34e714cc50b46adcde408cb0fcdc5f62a96a79a79be39f58af7eaae5`).
Windows Server 2022 used Clang/clang-cl 19.1.7, the official SDK and matching
RTTI OFF. Debug passed 21/22, including `value_structs_cli`; `frontend` ended
with CTest's generic SegFault classification after 0.86 seconds. Its archived
LastTest.log has **empty frontend output**. The artifact contains only that
test log and CMakeConfigureLog.yaml: no executable, exception code or crash dump.
Consequently that artifact alone cannot establish the failing subgroup or
distinguish Windows stack overflow from an access violation.

Before changing production code, the unchanged local Debug frontend reproduced
SIGSEGV (exit 139) with a 1 MiB process stack. GDB located it at entry to
`Semantics::expression_impl`, compiler.cpp:772 in that commit, recursively
called from the unary handler at line 879. The originating test is
`diagnostic_acceptance::run`, tests/frontend/diagnostics.hpp:141: nine constants,
each with forty unary operators, referencing the next constant. It expects
the combined semantic depth guard and an independent later-function diagnostic.
It is reached **before** `struct_tests`; the new bounded recursive/expanded
value-layout case therefore does not cause this reproduced crash.

Disassembly of the unmodified Clang 19 Debug object shows an 8,416-byte
`expression_impl` stack allocation (`sub rsp, 0x20e0`). Phase 12's construction,
field and expanded Type handling enlarged this single function's collection of
temporaries. Every recursive unary step reserved that whole frame, exhausting
the stack before the existing 256-level guard. This is a production resource
safety defect, not a reason to remove a depth test. The debugger's unreadable
arguments at the fault are at the exhausted function prologue, not evidence
that a null Semantics object was deliberately called.

Extracted literal, name, field, call, conversion, unary and binary checking into
small member handlers behind the same dispatcher, semantic model and guard.
No typing/evaluation order/diagnostic logic changed; a comparison against HEAD
confirmed all six extracted branch bodies match apart from whitespace and local
type aliases. The dispatch frame now reserves 288 bytes; the unary handler
reserves 496 bytes. The formerly crashing complete frontend suite now passes
on a 1 MiB stack with **both** installed and official SDK Debug builds, including
the combined-depth check and bounded expanded-layout check. No stack reserve,
resource limit, LLVM version, RTTI policy, sanitizer flag or assertion changed.

Added flushed stderr begin/pass records for each frontend group and markers for
the combined-depth and expanded-layout cases. Future Windows artifacts will
identify the last active group/case even on abrupt termination. Linux CI now
also repeats its existing Debug frontend suite under `ulimit -s 1024`; all 22
normal suites and platform jobs remain intact.

Verification (all builds completed without warnings):

| LLVM 19.1.7 SDK | Debug | Release | RelWithDebInfo ASan/UBSan |
| --- | --- | --- | --- |
| Installed `/usr/lib/llvm-19`, RTTI ON | 22/22, 24.74s | 22/22, 28.33s | 22/22, 40.72s |
| Complete official Linux archive, RTTI OFF | 22/22, 17.19s | 22/22, 24.64s | 22/22, 34.82s |

These are 132 suite executions, covering native/JIT/AOT normal and optimized
paths, value structs, GC/reload, diagnostics, shared analysis/tooling and LSP.
Both SDKs retain their headers, `libLLVMOrcJIT.a` and `LLVMConfig.cmake`;
llvm-config reports 19.1.7. Fresh configure runs linked and executed the real
ORC probe successfully against both. Installed-SDK sanitizers retain RTTI/vptr
coverage; the official no-RTTI SDK preserves the previously documented ABI
correction and coverage boundary.

Commands (from repository root; each tree was rebuilt with `-j 2`):

```sh
cmake --build build/ci-debug -j 2
ctest --test-dir build/ci-debug --output-on-failure -j 4
# Repeat for ci-release, ci-official-debug and ci-official-release.
env ASAN_OPTIONS=detect_leaks=1:halt_on_error=1:exitcode=87 \
    UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1:exitcode=86 \
    ctest --test-dir build/ci-sanitizers --output-on-failure -j 4
# Same strict environment for ci-official-san, outside the tracing sandbox.
bash -c 'ulimit -s 1024; ctest --test-dir build/ci-debug -R "^frontend$" -V'
bash -c 'ulimit -s 1024; ctest --test-dir build/ci-official-debug -R "^frontend$" -V'
cmake -S . -B build/ci-debug
cmake -S . -B build/ci-official-debug
git diff --check
```

The pre-fix reproduction used `ulimit -s 1024` followed by
`gdb -batch -ex run -ex bt --args build/ci-debug/void_tests frontend "$PWD"`.
Artifacts, original Windows job log/ZIP, full pre-fix backtrace, fresh configure
logs, six build/test logs, SDK checks and verbose small-stack results are saved
under `build/phase12-windows-investigation/` (ignored local evidence).

Windows attribution is strongly supported by the matching small-stack failure,
Debug-specific oversized frame and successful correction, but **is not a
Windows-native backtrace or verified Windows success**. This machine lacks the
Windows SDK/MSVC libraries and the Windows executable was not uploaded. Linux
small-stack execution does not reproduce the MSVC standard-library ABI. Another
hosted run must confirm Windows Debug/Release 22/22 and the remaining matrix
and editor steps. The added progress output will localize any remaining failure.
Ready for the owner's next CI attempt; no commits, branches, pushes or remote
modifications were made. The owner's untracked WRENCH_CLANKER.md was preserved.

## Phase 5 run #7 sanitizer ABI correction, 11 October 2026

Inspected the actual Ubuntu job `114414960372` in run `38120807013`, at commit
`384307dcdb65b476de587c4681e047b9f7dc3acc`, also the local unchanged baseline.
Windows/macOS and Ubuntu Debug/Release passed according to the user and job log;
Ubuntu sanitizers failed eight of ten suites with invalid-vptr reports in ORC
promise/future cleanup and shared code ownership cleanup.

Before editing, configured, built and ran the complete official Linux 19.1.7
archive with its own matching Clang/LLD and CI's RelWithDebInfo/ASan/UBSan settings.
The unchanged full suite reproduced the same eight failures: **2/10 passed in
2.42 seconds**. The host uses GCC 16 libstdc++ headers; CI uses GCC 14. Prior local
sanitizer validation used the installed RTTI-enabled SDK, whereas the official
archive has RTTI disabled. Earlier official-archive Debug validation did not
exercise this sanitizer/RTTI combination.

The archive's actual `LLJIT.cpp.o` bitcode contains a promise-result vtable with
null RTTI metadata. A separate two-file shared_ptr factory/consumer reproducer
fails cleanup with exit 86 when only the factory disables RTTI; consistent RTTI
passes with leak detection. Inspecting the emitted factory vtable confirms the
same absent metadata without relying on a corrupt allocation. CI_SANITIZER_ABI.md
records the mechanism, actual log link, commands and precise coverage limits.

Moved SDK RTTI selection from the backend-only target to shared C++ build options
for all linked targets, in every build type. The SDK configure probe uses the same
flag. The configure output discloses the no-RTTI ABI's unavailable vptr check.
LLVM 19.1.7, complete SDKs, dependencies, exceptions, sanitizer flags, strict
failure settings, entry function-type metadata and all ten tests are preserved.
No frontend, runtime implementation, IR lowering, test assertion or workflow
was changed. Separate installed-SDK validation retains RTTI/vptr coverage; no
equivalence of vptr coverage with the no-RTTI archive is claimed.

Completed local verification:

| SDK / configuration | Result | Full-suite elapsed |
|---|---|---|
| Installed 19.1.7, RTTI ON, Debug | 10/10 | 27.68 s |
| Installed 19.1.7, RTTI ON, Release | 10/10 | 25.35 s |
| Installed 19.1.7, RTTI ON, strict ASan/UBSan | 10/10 | 25.15 s |
| Complete official archive, RTTI OFF, Debug | 10/10 | 10.78 s |
| Complete official archive, RTTI OFF, Release | 10/10 | 9.01 s |
| Complete official archive, RTTI OFF, strict ASan/UBSan | 10/10 | 29.51 s |
| Complete official archive, strict sanitizer repeat, all suites three times | 30/30 executions | 89.26 s |

All 14 C++ translation units in each sanitizer configuration were checked against
their SDK RTTI setting; every C/C++ source command retains ASan/UBSan. The installed
CLI object retains dynamic-type/vptr handlers and the archive-based runtime object
retains indirect-function-type handlers. Seven deliberate fault controls verified
fatal ASan heap overflow, UBSan signed overflow and incorrect function calls in
both modes, plus an invalid downcast with RTTI. Combined runtime options yield
exit 86 for these controls, including the address report; this remains distinct
from expected source-error exit 1. Sanitizers ran outside the tracing sandbox
with leak detection enabled. Build/test/reproducer/control evidence is retained
under ignored `build/ci7-verification/`.

All six configurations completed actual builds and the full ten-suite execution.
The complete-archive builds were serial and produced no new linker backtrace;
positive build/test logs contain no ASan/UBSan/LeakSanitizer report. The existing
third-party `<ciso646>` deprecation warning remains visible where emitted.
A new hosted run is still required for Ubuntu 24.04/GCC 14 and the changed shared
C++ flags on Windows/macOS. A hosted vptr lane would require a complete
RTTI-enabled LLVM 19.1.7 SDK. No commit, push or remote modification was performed.

## Compiler foundation and architectural proofs, 10 October 2026

### Inputs and scope

Read the complete supplied Language Bible and Development Strategy before
implementation. Moved the original documents into `docs/LANGUAGE_BIBLE.md` and
`docs/DEVELOPMENT_STRATEGY.md`, preserving their snapshots and appending current
implementation notes. Followed the user's larger-phase instruction in place of
the older numbered milestone process. Built fresh code in `Void.Language`;
no earlier compiler implementation was reused or backup modified.

All work is local. No GitHub repository, Git initialization, commit, push,
publication or remote modification was performed. Per user direction, hosted
workflow execution is deferred until the user creates the repository and pushes.

### Implemented

- C++20/CMake targets for core frontend source support, backend-independent proof
  IR, LLVM lowering, managed/native runtime proofs, compiler harness and executable
  tests. No empty lexer/parser/LSP/task systems were added. `stdlib/README.md`
  records the future standard-library boundary without claiming an implementation.
- Exact LLVM 19.1.7 SDK requirement and matching Clang 19.1.7 AOT driver check.
  Verified real ORC headers, `libLLVMCore.a`, and `LLVMConfig.cmake` in the installed
  `/usr/lib/llvm-19` SDK. Missing dependencies were installed by the user; no
  dummy targets, mixed libraries or relaxed requirements were used.
- Source byte offsets, CRLF-aware line/column lookup and a basic source diagnostic.
  Validated proof IR with i32 functions, arguments, literals, arithmetic, Boolean
  comparisons, calls, branches, returns and stable module identities.
- Shared LLVM lowering with IR verification, O2 optimization, native object
  generation and source debug metadata. AOT integration links and executes a real
  native binary with three runtime inputs and verifies its DWARF source table.
- ORC invocation and resource-tracked generations. Leased dispatch preserves active
  old code, retains the execution session and safely removes unused generations.
  Signature/layout/capture/constant incompatibility is rejected without changing
  the active slot. Invalid IR and missing native symbols fail compilation.
- Experimental single-threaded precise non-moving heap, copied type descriptors,
  nested-reference offsets, explicit roots, iterative marking, cycle reclamation
  and validation before sweep. LLVM-generated normal/O2 loops register a stack
  slot and preserve live references across repeated native safepoints.
- Real C ABI fixture: generated native calls, validated borrowed terminated UTF-8,
  opaque resource ownership, stable retained callback, rooted managed capture,
  unregister and deterministic resource release. Callback behavior follows a
  compatible JIT replacement without registering a new native function address.
- Local `.github/workflows/ci.yml` for Linux, Windows and macOS, Debug and Release,
  with Linux ASan/UBSan. Downloads complete official versioned LLVM SDK archives;
  configures Windows SDK/MSVC ABI and Darwin SDK explicitly; installs Linux
  development dependencies. Push, pull-request and manual triggers; read-only
  repository permissions; standard hosted runners only.
- Comprehensive ignore rules for build products, credentials, local environments,
  IDE configuration, agent directories and cloud configuration. `docs/` and
  `.github/` remain included.

### Verified host and toolchain

| Item | Actual value |
|---|---|
| Executed OS | PikaOS 4, Debian-family Linux |
| Architecture | x86_64 |
| LLVM development SDK | 19.1.7, installed distribution package |
| C and C++ compiler | Clang/Clang++ 19.1.7 |
| LLVM CMake directory | `/usr/lib/llvm-19/lib/cmake/llvm` |
| CMake | 4.3.4 |
| Local build generator | Unix Makefiles |
| AOT linker actually used by Clang | GNU ld 2.47 |
| Additional available linker | LLD 19.1.7, verified installed, not used in recorded AOT links |
| C++ standard library | Host GCC 16 libstdc++ headers/runtime |
| Windows / macOS / GitHub Actions | Not executed, unverified |

Linux package reproduction is in the README. Required base packages include
`llvm-19-dev`, `clang-19`, `lld-19`, CMake, a build generator, and the SDK's
exported development dependencies. The user additionally installed `libzstd-dev`
and `libedit-dev` after CMake identified missing zstd targets. LLVM configuration
still notes optional CURL is absent; the selected components do not require it.

A noninteractive administrator install was unavailable. Matching packages were
initially downloaded to `/tmp` during toolchain investigation, but they were not
used for the project builds. All recorded configurations use the complete
system-installed SDK after the user's installation.

### Actual commands and results

Run from the project root. The recorded builds used the default Unix Makefiles
generator; Ninja is also the configured hosted-CI generator, but has not run here.

```sh
cmake -S . -B build/debug \
  -DLLVM_DIR=/usr/lib/llvm-19/lib/cmake/llvm \
  -DCMAKE_C_COMPILER=clang-19 -DCMAKE_CXX_COMPILER=clang++-19 \
  -DCMAKE_BUILD_TYPE=Debug
cmake --build build/debug -j 4
ctest --test-dir build/debug --output-on-failure

cmake -S . -B build/release \
  -DLLVM_DIR=/usr/lib/llvm-19/lib/cmake/llvm \
  -DCMAKE_C_COMPILER=clang-19 -DCMAKE_CXX_COMPILER=clang++-19 \
  -DCMAKE_BUILD_TYPE=Release
cmake --build build/release -j 4
ctest --test-dir build/release --output-on-failure

cmake -S . -B build/sanitizers \
  -DLLVM_DIR=/usr/lib/llvm-19/lib/cmake/llvm \
  -DCMAKE_C_COMPILER=clang-19 -DCMAKE_CXX_COMPILER=clang++-19 \
  -DCMAKE_BUILD_TYPE=RelWithDebInfo -DVOID_SANITIZERS=ON
cmake --build build/sanitizers -j 4
ctest --test-dir build/sanitizers --output-on-failure

ctest --test-dir build/release --output-on-failure \
  --repeat until-fail:10 -R 'reload|gc|native'
python3 -m py_compile tools/ci/llvm_sdk.py
```

Final results after code/test changes:

| Configuration/check | Executed result |
|---|---|
| Debug | 6/6 suites passed, 0.59 seconds |
| Release | 6/6 suites passed, 0.53 seconds |
| RelWithDebInfo + ASan/UBSan, including leak detection | 6/6 suites passed, 1.33 seconds |
| Release repeat stress | 10 executions each of reload, GC and native suites passed, 3.54 seconds total |
| Official SDK helper Python syntax | Passed |
| Standalone ORC harness | Executed, printed `42` |
| AOT runtime paths | Executed, printed `7`, `42`, `43` as expected |
| Emitted object debug information | `llvm-dwarfdump --verify` passed; VOID source line table present |

Each suite executes compiled code:

| Suite | Observable checks |
|---|---|
| core | Source positions/diagnostics and rejection of invalid operands, types, duplicate definitions and missing callees |
| jit | Normal/O2 native results and branches; invalid entry; generation retirement; execution after host destruction |
| reload | Active old call blocks on another thread; future calls use new code; old call returns original result; code retires after return; incompatible/invalid edits leave active code intact; four caller threads and 100 compatible publications |
| gc | Embedded reference trace, malformed layouts, invalid-root sweep prevention, mark rollback, normal/O2 generated root loops, live cycle survival and dead cycle reclamation |
| native | Real JIT-to-C call, missing-symbol failure, UTF-8 and NUL diagnostics, opaque resource lifetime, retained/rooted C callback following replacement and unregister cleanup |
| aot | LLVM object emission, Clang native linking, debug metadata verification, executable runtime branch and arithmetic outputs, paths containing spaces |

Each GC suite execution performs 20,000 generated safepoint collections across
40 normal/O2 invocations, allocating 40,000 unreachable cycle nodes plus the
rooted graphs. Each reload suite includes 100 stress publications and four
concurrent native callers. The repeat run adds 1,000 compatible publications and
200,000 generated safepoint collections. These are correctness stress counts,
not throughput or pause-latency performance claims.

### Failures encountered and resolved

- Initial machine had LLVM 23 development tools without usable development SDK
  headers. Selected and installed the pinned complete LLVM 19.1.7 SDK instead.
- LLVM exported zstd dependency could not resolve until the user installed the
  development package. Retried normal CMake configuration successfully.
- Corrected an LLVM 19 header path during compilation; fixed project aggregate
  initialization warnings with explicit default member initialization.
- The first sandboxed sanitizer run failed because LeakSanitizer cannot inspect
  processes under the sandbox tracing restriction. Reran outside that restriction
  with leak detection enabled; all suites passed. Sanitizers were not disabled.
- One third-party warning remains: LLVM 19 includes `<ciso646>`, whose host GCC 16
  header warns under C++20. No global warning suppression was added.

### Decisions, limits and next phase

Architectural choices and their rationale are in `DECISIONS.md`. Restrictions
and unverified work are in `KNOWN_ISSUES.md`. This phase establishes executable
foundation proofs, not the full GC, source compiler or hot reload product.
LLVM statepoint/stack-map integration was investigated through installed APIs
but is not implemented or validated. No performance benchmark, TSan run, foreign
thread callback test or non-Linux execution is claimed.

The next cohesive phase should specify and implement a small frontend with exact
source diagnostics, lower parsed functions into the current IR, and test their
AOT/JIT agreement. Resolve open grammar/numeric choices deliberately. Complete
optimized root lowering and thread-coordination proofs before adding managed
language types. Run hosted platform jobs once the user publishes the repository.

## CI stabilization phase: first hosted run, 10 October 2026

User request: fix actual CI failures before expanding the frontend, keep LLVM
19.1.7 and all architectural tests, work locally without commits or pushes.

### Hosted evidence inspected

Read the actual job logs of
[run 38098620186](https://github.com/Shmellyorc/Void.Language/actions/runs/38098620186),
commit `8ed7e395bd4789f4cb82c61a346794f37a435077`:

| Host/job | Observed failure |
|---|---|
| Ubuntu 24.04 / 114349692845 | Clang invoked GNU ld, which rejected `libLLVMOrcJIT.a` as an unrecognized file format |
| Windows 2022 / 114349692876 | Ninja required the producer's absent VS2019 Professional DIA SDK `diaguids.lib`, despite the runner having VS2022 |
| macOS / 114349692735 | libc++ did not provide `std::jthread` or `std::stop_token` in the selected configuration |

All three configured successfully, then failed Build Debug; none executed the
architectural tests. Earlier sections above describe the earlier local phase,
not successful hosted verification.

### Corrections

- The SDK helper verifies and exports the Linux archive's LLD 19.1.7. All CI
  configurations pass it explicitly to CMake; both host executables and the AOT
  integration link use that exact linker. GNU ld is not given LLVM bitcode.
- CMake resolves only missing imported LLVM DIA dependencies to a real library
  in the active Visual Studio SDK, with the selected target architecture. It
  retains the dependency and fails if it cannot be found. No SDK files are edited.
- The reload test uses standard threads, an atomic stop flag and a scope guard
  that joins successfully created readers even on exceptions. Four concurrent
  callers and 100 publications remain, and exception cleanup is exercised.
- Configuration now builds and executes a minimal ORC host against the selected
  SDK. Testing the exact Linux archive found an additional undefined LLVM RTTI
  symbol: that SDK has `LLVM_ENABLE_RTTI=OFF`. The backend and probe now match
  this setting while retaining exceptions, C++20 and other targets' RTTI.
- CI preserves CMake configure diagnostics as well as CTest logs on failure.

### Local evidence

The installed `/usr/lib/llvm-19` SDK, Clang and LLD are 19.1.7. After the linker,
DIA and thread changes: Debug 6/6 passed (0.55 s), Release 6/6 (0.54 s), and
RelWithDebInfo ASan/UBSan 6/6 (1.41 s), with leak detection enabled outside the
sandbox tracing restriction. Ten repetitions each of reload/GC/native passed
(30 suite executions, 3.56 s). These SDK libraries are native ELF archives.

A synthetic imported LLVM target on Linux reproduced a stale DIA path. The
resolver replaced it with an existing compiled fixture library, and an executable
linked and called that fixture successfully. This checks CMake property repair,
not the Windows SDK, architecture or ABI; Windows hosted execution is pending.

Downloaded and extracted the exact official `LLVM-19.1.7-Linux-X64.tar.xz` used
by CI into the ignored `.toolchains/ci-investigation/` directory. Its static
archive member magic is LLVM bitcode (`BC c0 de`). A configuration using this
archive and the default GNU linker failed with the same unrecognized-format
error (at `libLLVMCore.a` in the SDK probe). The matching archive's LLD passed
that error and exposed the RTTI mismatch; after matching `LLVM_ENABLE_RTTI`,
the configure-time ORC executable linked and ran successfully.

Reproduction for the official archive (substitute its extracted root):

```sh
void_sdk="$PWD/.toolchains/ci-investigation/LLVM-19.1.7-Linux-X64"
cmake -S . -B build/ci-official-debug \
  -DLLVM_DIR="$void_sdk/lib/cmake/llvm" \
  -DCMAKE_C_COMPILER="$void_sdk/bin/clang" \
  -DCMAKE_CXX_COMPILER="$void_sdk/bin/clang++" \
  -DVOID_CLANG="$void_sdk/bin/clang" \
  -DVOID_DWARFDUMP="$void_sdk/bin/llvm-dwarfdump" \
  -DVOID_LINKER="$void_sdk/bin/ld.lld" -DCMAKE_BUILD_TYPE=Debug
cmake --build build/ci-official-debug --parallel 3
ctest --test-dir build/ci-official-debug --output-on-failure
```

Linux CI installs `ninja-build libffi-dev libedit-dev libxml2-dev libzstd-dev
zlib1g-dev libtinfo-dev`; the complete SDK provides LLVM/Clang/LLD. Distribution
SDK users additionally install the exact `llvm-19-dev clang-19 lld-19` versions
and the SDK's exported dependencies (including `libz3-dev` on the verified host).
No required dependencies or architectural tests are removed.

### Final verification after the SDK ABI correction

| Local configuration/check | Result |
|---|---|
| Exact official Linux CI archive + matching Clang/LLD, Debug | Full build and 6/6 executable suites passed, 0.52 s |
| Exact archive standalone ORC harness | Printed `42` |
| Installed SDK + matching Clang/LLD, Debug, final CMake | Configure probe, build and 6/6 passed, 1.27 s |
| Installed SDK + matching Clang/LLD, Release, final CMake | Configure probe, build and 6/6 passed, 1.82 s |
| Installed SDK ASan/UBSan | 6/6 passed with leak detection, as recorded above |
| Installed SDK repeated Release reload/GC/native | All 30 executions passed, as recorded above |
| SDK helper syntax and whitespace checks | Passed |

The final installed-SDK checks ran while the archive's LTO links were active;
these timings are observations, not performance comparisons. The official SDK
configure probe took 85 seconds and full executable linking took several minutes.
Only Debug was rebuilt locally against the exact archive; the hosted gate must
also confirm that archive's Release and sanitizer configurations. The distribution
SDK's Debug/Release/sanitizer results do not replace that hosted verification.

### Hosted gate still pending

After the user commits/pushes these changes, run all three jobs again. Required
results are Debug and Release builds plus all six executable suites on each
platform, and Linux ASan/UBSan. Specifically verify Windows DIA discovery,
release CRT/iterator ABI, native C/AOT/debug metadata and JIT execution; verify
macOS portable thread cleanup and native ORC/AOT execution with the Xcode SDK.
Linux runner memory, LTO link time and system dependency compatibility also need
hosted confirmation. No hosted green result is claimed from local tests.

## Windows AOT library discovery: second hosted run

Inspected [run 38099707063](https://github.com/Shmellyorc/Void.Language/actions/runs/38099707063),
commit `dc158ccc488436eb08044566ecd71cd68bd67b07`, including the actual logs of
Windows job `114352920880`, Linux job `114352921025` and macOS job `114352921035`.

| Hosted result | Evidence |
|---|---|
| Linux Debug / Release / ASan+UBSan | All 6/6 passed in each configuration (0.54 / 0.39 / 1.16 s) |
| macOS Debug / Release | All 6/6 passed in each configuration (0.85 / 0.78 s) |
| Windows Debug build | Passed, including DIA resolution and the executable SDK probe |
| Windows Debug tests | core/JIT/reload/GC/native passed; AOT failed before invoking the native linker |
| Windows Release | Configure, build and tests skipped after Debug AOT failure |

The Windows test emitted its object, then Clang reported:
`no such file or directory: 'legacy_stdio_definitions.lib'`. The job's `LIB`
already contained MSVC `14.44.35207/lib/x64`, UCRT and Windows SDK x64 library
directories. This was a driver argument error, not evidence of an absent SDK.
The GNU-style Clang driver validates a bare `.lib` as a local input file before
native linker library discovery can run.

Changed only the Windows library argument in `tests/integration/aot.cmake` to
`-llegacy_stdio_definitions`. Clang translates this library request for the MSVC
target, and the native linker searches the active developer environment. No
hardcoded SDK paths, copied libraries, dropped dependencies or skipped tests.
The direct generated-code `printf` call and the executable outputs `7`, `42`,
`43`, paths containing spaces and object debug checks remain intact. LLVM is
still exactly 19.1.7. The existing workflow already establishes the MSVC/Windows
SDK environment; it needs no change for this correction.

### Local verification

- Clang 19.1.7 targeting `x86_64-pc-windows-msvc` reproduced the bare-library
  diagnostic. With `-### -llegacy_stdio_definitions`, it instead produced a
  Windows linker invocation containing `legacy_stdio_definitions.lib`, with no
  input-file error.
- Compiled two small C sources into real x64 COFF objects, archived one as
  `sdk_probe.lib`, and linked a PE executable using Clang/LLD 19.1.7 with
  `-lsdk_probe` and `LIB` pointing to a temporary directory containing spaces.
  Removing that `LIB` path made linking fail for the required library. This
  checks library discovery and missing-dependency behavior, not Windows runtime
  execution. This separate fixture does not replace or modify the AOT proof.
- Linux Debug against the exact official CI archive: 6/6 suites passed, 0.52 s.
- Linux Release against the installed LLVM 19.1.7 SDK: 6/6 passed, 0.50 s.
  Both include actual native AOT linking, debug verification and execution.
- Whitespace/diff checks passed. The change is an integration script argument;
  the local compiled sources are unchanged and needed no rebuild.

Driver-check reproduction, using an existing object:

```sh
clang-19 --target=x86_64-pc-windows-msvc -fuse-ld=lld -### \
  proof.obj legacy_stdio_definitions.lib
# Reproduces the local input-file error when no such file exists in this directory.
clang-19 --target=x86_64-pc-windows-msvc -fuse-ld=lld -### \
  proof.obj -llegacy_stdio_definitions
# Forwards the required .lib to the Windows linker instead.
```

### Remaining hosted verification

A new run after the user commits/pushes must verify Windows Debug AOT discovery
of the actual MSVC compatibility library, native `printf` resolution, debug
metadata and all three executable outputs. Windows Release configure/build and
all six suites must then execute successfully for the first time. Linux/macOS
should continue passing the full matrix. Local cross-target driver/linker tests
cannot establish Windows SDK availability or native execution success.

No commits, pushes, hosted reruns or other remote mutations were performed. The
pre-existing user edit to `docs/LANGUAGE_BIBLE.md` was left untouched.


## First real source compiler — 10 October 2026

### Scope and foundation

The user confirmed all three GitHub Actions platforms passed the previous
foundation before authorizing this cohesive compiler phase. Read the current
Language Bible, strategy, decisions and known issues, and preserve those design
sources. The Language Bible was not edited. No commits, pushes, hosted reruns or
other remote changes were made.

Actual files now pass through UTF-8 source handling, indentation tokenization,
AST parsing, signature/binding/type/control-flow analysis and lowering to the
existing backend-independent IR. Shared LLVM lowering generates both ORC code
and native objects. The implementation supports multiple typed parameters,
i32/bool, recursive/forward calls, arithmetic/comparison/short-circuit Boolean
expressions, mutable/default-initialized locals, assignment/compound/postfix
updates, return, nested conditionals, conditional/unbounded for and nearest-loop
break/continue. It diagnoses while with the correct for replacement.

Extend IR signatures, typed slots and jumps without changing the old proof
contracts. Preserve all six architectural suites and their assertions, including
native printf, generated roots under optimization, concurrent replacement and
safe retirement. The new frontend suite shares the existing test runner, avoiding
another expensive static LLVM link in CI. Native-link script setup is shared by
the old AOT proof and new source integration test; required Windows libraries
remain intact. The existing workflow automatically executes all eight suites.

Documented provisional choices include indentation/identifier scope, expression
precedence, flow analysis, left-to-right evaluation and initial wrapping i32
arithmetic. No broader language grammar or numeric policy is silently finalized.
See SOURCE_COMPILER.md and DECISIONS.md for details and remaining questions.

### Actual source acceptance

`examples/architecture.void` is read by the compiler, not reconstructed as proof
IR. Both normal and O2 JIT/AOT execution produced:

| Input | Source ORC JIT | Linked native AOT |
| --- | --- | --- |
| -1 | 7 | 7 |
| 0 | 42 | 42 |
| 1 | 43 | 43 |
| 10 | 52 | 52 |

The source integration suite compiles three real files (`architecture`, `loops`,
`functions`) with and without O2, verifies actual source filenames in valid DWARF,
links native executables and compares 56 JIT/AOT results. It uses source/output
paths with spaces and rejects malformed/out-of-range native CLI arguments.
An edited copy changes 42 to 13 and renames calculate to main; both engines return
23 for input 10, checking real source behavior and native adapter symbol isolation.
Thirteen additional independent programs cover all arithmetic, bool types,
short circuiting around dangerous dynamic division, multi-argument short-circuit
CFGs, recursive/mutual calls, loop nesting/exits and local reinitialization.

Over fifty diagnostic/boundary checks cover indentation/tabs, unexpected tokens,
unresolved names, duplicates/shadowing, argument counts, exact types, invalid and
missing returns, loop misuse, unsupported iteration/numbers, overflow literals,
zero literals, malformed UTF-8 and source locations/carets. BOM, Unicode comments,
scalar columns, CRLF/CR and missing EOF newline receive positive checks. The JIT
host rejects incompatible entry ABIs before invoking generated code.

### Sanitizer finding and correction

The expanded executions exposed a Clang UBSan indirect-call metadata read before
a page-aligned ORC entry. This was an actual JIT host boundary fault under ASan.
The fix obtains the i32(i32) function metadata from the actual host compiler at
configuration and attaches LLVM func_sanitize metadata before ORC code generation.
No sanitizer check or architectural assertion is suppressed. Application-generated
instructions remain outside sanitizer instrumentation, as in the original proofs;
the compiler/runtime, native fixture and host boundary retain instrumentation.

LeakSanitizer requires process inspection unavailable under this desktop's tracing
sandbox. Sanitizer CTest executions ran outside that sandbox with leak detection
and all checks enabled.

### Reproduction and local verification

The installed SDK is `/usr/lib/llvm-19/lib/cmake/llvm`, with Clang and LLD 19.1.7.
The required Linux packages remain documented in README; official-archive CI
installs ninja-build, libffi-dev, libedit-dev, libxml2-dev, libzstd-dev, zlib1g-dev
and libtinfo-dev. SDK-specific development dependencies must remain installed.

```sh
cmake -S . -B build/ci-debug \
  -DLLVM_DIR=/usr/lib/llvm-19/lib/cmake/llvm \
  -DCMAKE_C_COMPILER=clang-19 -DCMAKE_CXX_COMPILER=clang++-19 \
  -DVOID_LINKER=/usr/lib/llvm-19/bin/ld.lld -DCMAKE_BUILD_TYPE=Debug
cmake --build build/ci-debug -j3
ctest --test-dir build/ci-debug --output-on-failure

cmake -S . -B build/ci-release \
  -DLLVM_DIR=/usr/lib/llvm-19/lib/cmake/llvm \
  -DCMAKE_C_COMPILER=clang-19 -DCMAKE_CXX_COMPILER=clang++-19 \
  -DVOID_LINKER=/usr/lib/llvm-19/bin/ld.lld -DCMAKE_BUILD_TYPE=Release
cmake --build build/ci-release -j3
ctest --test-dir build/ci-release --output-on-failure

cmake -S . -B build/ci-sanitizers \
  -DLLVM_DIR=/usr/lib/llvm-19/lib/cmake/llvm \
  -DCMAKE_C_COMPILER=clang-19 -DCMAKE_CXX_COMPILER=clang++-19 \
  -DVOID_LINKER=/usr/lib/llvm-19/bin/ld.lld \
  -DCMAKE_BUILD_TYPE=RelWithDebInfo -DVOID_SANITIZERS=ON
cmake --build build/ci-sanitizers -j3
ctest --test-dir build/ci-sanitizers --output-on-failure
ctest --test-dir build/ci-sanitizers --output-on-failure --repeat until-fail:5
```

Debug: 8/8 passed (3.19 s). Release: 8/8 passed (1.91 s).
ASan/UBSan: all eight suites passed five consecutive executions after the final
changes (40 suite executions, 65.56 s while the official SDK linked concurrently).
ASan, UBSan function checks and leak detection remained enabled.

The complete official Linux CI archive is also configured locally:

```sh
cmake -S . -B build/ci-official-debug \
  -DLLVM_DIR="$PWD/.toolchains/ci-investigation/LLVM-19.1.7-Linux-X64/lib/cmake/llvm" \
  -DCMAKE_C_COMPILER="$PWD/.toolchains/ci-investigation/LLVM-19.1.7-Linux-X64/bin/clang" \
  -DCMAKE_CXX_COMPILER="$PWD/.toolchains/ci-investigation/LLVM-19.1.7-Linux-X64/bin/clang++" \
  -DVOID_LINKER="$PWD/.toolchains/ci-investigation/LLVM-19.1.7-Linux-X64/bin/ld.lld" \
  -DCMAKE_BUILD_TYPE=Debug
cmake --build build/ci-official-debug -j3
ctest --test-dir build/ci-official-debug --output-on-failure
```

The official SDK configuration links/runs the ORC probe, uses its bitcode archives
through matching LLD, and matches its disabled RTTI ABI. The final rebuild passed
all eight suites (1.70 s), including native source compilation/link/execution and
all preserved architectural regressions. Whitespace checks and local documentation
links passed. The source entry ABI diagnostic was also checked manually at its
function-name location (line 1, column 6).

Native source demonstration (after the integration tests create the executable):

```sh
build/ci-debug/voidc run examples/architecture.void 10
# 52
"build/ci-debug/source programs with spaces/architecture-optimized" 10
# 52
```

### Remaining verification and next phase

The expanded Windows/macOS Debug/Release and hosted Linux sanitizer runs remain
unverified until the user commits/pushes and runs Actions. The prior foundation
passes do not establish these new frontend results on those hosts. Keep LLVM
19.1.7 pinned, the required SDK libraries and every architectural suite intact.

Next, after that hosted gate: module/import resolution with stable identity and
multi-file diagnostics, deliberate numeric and executable/export conventions,
and diagnostic/runtime error improvements. Managed-language types must wait for
root/thread integration; structs, generics, collections and async remain deferred.


## Module system and multi-file compilation — 10 October 2026

### Starting point and delivered architecture

The user confirmed the first real source compiler passed Linux/Windows/macOS
Actions Debug/Release and Linux sanitizers. Started from the clean existing local
project, read the Bible/strategy/source/compiler decision and issue records, and
kept LLVM 19.1.7 and the original eight suites. No Language Bible edits, commits,
pushes, hosted reruns or other remote mutations were made.

The CLI now calls compile_project for check/run/emit. A source graph establishes
its root at the entry file's parent, normalizes VFS imports, retains one source
snapshot per canonical module, parses the reachable graph and registers every
function signature before checking bodies. The in-memory source API shares the
same graph/type/lowering implementation, with an explicit diagnostic for imports
without a filesystem project context. One validated IR compilation unit feeds the
existing shared LLVM JIT/AOT implementation.

Implemented root-relative, importer-relative and aliased use declarations;
canonical deduplication; deterministic simple/qualified call resolution; default
public, explicit public/internal and file-private functions; function-only cycles
and cross-file recursion. Source function ownership carries the VFS module key
and source name into IR; native symbols encode those keys injectively. LLVM debug
names remain friendly, with imported source filenames/locations retained. The
entry adapter selects only the entry file's own function, never an imported entry.
Final review separated the physical compilation-unit debug path from virtual
module identity; the native suite reads DW_AT_comp_dir and verifies the actual
source directory rather than merely matching a filename in line tables.

Documented deliberate initial policies in MODULE_SYSTEM.md and DECISIONS.md:
exact-case portable filenames, space-containing paths, dot/separator normalization,
no root escapes/packages, and rejection of imported symlinks/redirects or hard-link
aliases. Project-scoped identities survive physical relocation and alias changes;
persistent identities for independent projects/external libraries remain deferred.
Explicit aliases expose qualified functions only; direct unaliased imports expose
simple names and optional filename-stem qualification. Ambiguous simple calls
require qualification/aliasing, and implicit qualifier ambiguity is diagnosed only
when used. Repeated identical imports are idempotent, not duplicate declarations.

No namespace hierarchy, folder-based visibility, global initialization or second
compiler/backend path was introduced. Existing arithmetic, Boolean short circuit,
control-flow/for rules and architectural GC/native/reload proofs remain intact.

### Actual multi-file source acceptance

Created the requested real source files:

```text
examples/modules/main.void
examples/modules/math/calculator.void
examples/modules/helpers/numbers.void
```

The unchanged acceptance program calls calculator.double(value) and
increment(value) from their declaring files. Normal and O2 source JIT/native AOT
results matched:

| Input | Multi-file ORC JIT | Linked native AOT |
| --- | --- | --- |
| 0 | 1 | 1 |
| 1 | 4 | 4 |
| 10 | 31 | 31 |
| -1 | -2 | -2 |

The module native integration suite copies that actual project into paths with
spaces, checks/emits/links/verifies DWARF, and executes it from an unrelated cwd.
It verifies line tables for main.void, calculator.void and numbers.void. Independent
nested and cyclic projects run through the same normal/O2 JIT/AOT sequence.
Changing only calculator.void from multiplication by 2 to multiplication by 3
changes results to 1, 5, 41 and -3 for the same inputs in both engines. This is
actual imported-source compilation, not reconstructed IR or a resolver-only proof.
The four graph variants provide 64 native result comparisons, plus relative-entry
cwd and imported-error diagnostic checks.

### New tests and preserved regressions

`modules` and `modules_cli` bring CTest to ten suites; the original eight names,
assertions and architectural behaviors remain. Both suites are part of every
existing workflow configuration; no workflow or LLVM dependency was weakened.

The module runner executes twelve positive graph scenarios with normal/O2 ORC:
nested/relative/normalized/repeated imports, multiple aliases to one owner,
identical stems with distinct simple functions, same-name functions in different
files, private helpers, public/internal visibility across folders, mutual recursion,
self-imports, local declaration precedence, for mutation, space-containing paths,
multiple typed arguments/short-circuiting and imported entry isolation. Additional
relocation/replacement checks preserve the compatibility key and old native leases.

More than forty negative graph cases check missing files, invalid paths/root
escapes, packages/OS drives, aliases/ambiguity, private access, counts/types,
non-transitive exports, duplicate declarations, imported semantic/syntax/UTF-8
errors, directories, exact filename case, case collisions, symlinks and hard-link
aliases. Diagnostics assert the actual originating filename, line, column and
source caret, without LLVM/internal symbol noise. This Linux filesystem supported
real distinct case-only files, symlinks and hard links, so those negative fixtures
executed. On hosts unable to create those filesystem objects, the runner reports
the capability explicitly; normal source graph and exact-case tests still execute.

### Commands and local results

Retained build caches use Clang/LLD/LLVM 19.1.7. Installed SDK:
`/usr/lib/llvm-19/lib/cmake/llvm`. Fresh configuration commands and Linux development
packages remain in README and the previous phase's reproduction section.
Actual rebuild/test commands for this phase:

```sh
cmake --build build/ci-debug -j3
ctest --test-dir build/ci-debug --output-on-failure
cmake --build build/ci-release -j3
ctest --test-dir build/ci-release --output-on-failure
cmake --build build/ci-sanitizers -j3
ctest --test-dir build/ci-sanitizers --output-on-failure
ctest --test-dir build/ci-sanitizers --output-on-failure \
  -R '^(modules|modules_cli|reload|gc|native)$' --repeat until-fail:5
# Repeat module suites after the final debug-source-path correction.
ctest --test-dir build/ci-sanitizers --output-on-failure \
  -R '^(modules|modules_cli)$' --repeat until-fail:5
cmake --build build/ci-official-debug -j3
ctest --test-dir build/ci-official-debug --output-on-failure
```

Debug: 10/10 passed (3.79 s). Release: 10/10 passed (9.19 s, concurrent SDK builds).
An initial sanitizer run passed 10/10 (17.72 s); modules/module-native/reload/GC/
native then passed five consecutive runs each (25 suite executions, 66.93 s).
After the final debug-source-path correction, ASan/UBSan passed 10/10 (7.16 s);
both module suites passed five consecutive runs each (10 suite executions, 15.97 s).
The final complete official LLVM SDK Debug build passed 10/10 (2.80 s), including
the physical compilation-unit directory check, native graph execution and all
architectural regressions. Whitespace and documentation-link checks passed.
Sanitizer CTest runs outside this desktop's tracing sandbox so LeakSanitizer can
inspect processes. ASan, UBSan function checks and leak detection stay enabled.
The complete official CI SDK build uses its own Clang/LLD 19.1.7 and bitcode
libraries, preserving its RTTI ABI and the executable ORC configuration probe.

For a standalone native demonstration using the built compiler:

```sh
build/ci-debug/voidc run examples/modules/main.void 10
# 31
build/ci-debug/voidc emit examples/modules/main.void -o build/ci-debug/modules-demo.o
clang-19 -fuse-ld=/usr/lib/llvm-19/bin/ld.lld \
  build/ci-debug/modules-demo.o -o build/ci-debug/modules-demo
build/ci-debug/modules-demo 10
# 31
```

### Remaining hosted verification and next phase

Windows/macOS Debug/Release and hosted Linux ASan/UBSan execution of the ten-suite
matrix remain unverified until the user commits/pushes and runs Actions. The prior
eight-suite hosted success does not establish these new changes. Check actual
Windows SDK linking, cross-platform owned native symbols, exact-case handling,
filesystem capability reporting and multi-file DWARF on those runners.

Next, after that gate: const/project-root compile-time constants, precise numeric
policy/conversions and source-first runtime diagnostics, with deliberate constant
dependency/initialization semantics. Project/export configuration and diagnostic
recovery can build on these identities. Packages and external imports need explicit
identity/permission contracts; managed types still require the GC root/thread gate.

## Constants, numeric correctness and runtime diagnostics, 10 October 2026

The user reports the preceding ten-suite module phase passed GitHub Actions on
Linux, Windows and macOS. That is the starting gate for this phase, not hosted
verification of the following local changes. No commits, pushes, publications or
remote mutations were made. LLVM remains exactly 19.1.7; the Bible is unchanged.

### Implementation and architecture

Added i32/bool local readonly const with runtime initializers and module-level
pure compile-time constants, optional exact annotations, safe forward references,
dependency-cycle diagnostics, direct/qualified import lookup, visibility and
project-root constant access without importing the entry. Constants/functions share
file declaration ownership. Root-name collisions and local shadowing diagnose
before LLVM. Assignments, compounds and increment/decrement reject readonly values.

The existing expression type checker annotates constant initializers. A small
pure evaluator shares operation-to-IR definitions, short-circuits execution and
uses unsigned wrapping arithmetic plus widened signed division. Every declaration
and expression operand remains validated; structural dependency cycles and impure
initializers fail even behind lazy operators. No runtime module initialization,
second type checker, wider numeric types or managed readonly system was added.
Unary negation of an already valid negative boundary literal now lowers as an
operation instead of constructing an out-of-range positive literal.

Source IR retains source locations and relevant source-line/caret snapshots.
Shared LLVM lowering supplies a hidden per-invocation failure pointer to source
functions. Division/remainder by zero record a source diagnostic and return
normally; actual callers propagate failure and append their source call sites.
The trace includes at most 32 callers and explicitly reports omitted callers.
The JIT copies generation-owned strings and raises a host exception only after
native return. AOT's native main prints the same diagnostic metadata and exits 1.
There is no C++ unwind through generated code, TLS/global error state or foreign
stack guessing. Const-containing source snapshots form an exact conservative
restart compatibility token, including imported files and local runtime const.

VALUE_SEMANTICS.md documents lookup/cycle rules, current arithmetic, the boundary,
AOT stdout/JIT stderr difference, conservative restart restrictions, unimplemented
exceptions/cleanup and unapproved future literal/conversion recommendations.
SOURCE_COMPILER, MODULE_SYSTEM, DECISIONS, KNOWN_ISSUES, strategy and README were
updated without replacing the source-of-truth design snapshots.

### Actual native acceptance results

Compiled the real examples/constants/main.void and game/scoring.void using forward
root constants and runtime local const. No special-case IR was reconstructed.
Both normal and O2 compilation produced these results in both native engines:

| Input | JIT | Linked AOT executable |
|---:|---:|---:|
| 0 | 50 | 50 |
| 1 | 52 | 52 |
| 10 | 70 | 70 |
| -1 | 48 | 48 |

The standalone demo emitted `build/ci-debug/values-demo-normal.o` and
`values-demo-O2.o`, linked them with matching Clang/LLD 19.1.7, and ran all inputs.
The runtime-error example's JIT and linked `values-demo-error` both exited 1 with:

```text
.../examples/runtime_errors/arithmetic.void:2:16: runtime error: Division by zero
    return 100 / value
               ^
In function /arithmetic.void::calculate
Called from /main.void::bridge
.../examples/runtime_errors/main.void:4:23: source call
    return arithmetic.calculate(value)
                      ^
Called from /main.void::entry
.../examples/runtime_errors/main.void:7:12: source call
    return bridge(value)
           ^
```

### Expanded tests, with all ten suites preserved

Existing core/JIT/reload/GC/native/AOT assertions and frontend/module tests remain.
Extended frontend, modules and modules_cli rather than disabling or replacing any
suite. New tests exercise readonly mutations, initialization/types, forward and
cyclic constants, root/private/internal/direct/aliased lookup, name collisions,
function-only cycles with safe constants and restart rejection for root/imported
local const edits. Concurrent failure tests use four callers, with repeated failing
and successful invocations to verify per-call isolation.

An independent widened-integer oracle covers eight boundary/signed operands per
side and five operators, excluding zero divisors for division/remainder: 304 cases.
Frontend tests make 1,216 comparisons against constant and normal/O2 JIT results.
A generated real `.void` matrix tests all 304 against native runtime arithmetic and
constant values in normal/O2 JIT and linked AOT. Real numeric.void independently
checks compounds, negation, MIN/-1, wraparound, signed division/remainder and lazy
constant arithmetic. Real-source native integration covers division, remainder,
both compounds, bool/no-result propagation, recursive trace truncation and imported
caller positions. AOT tests edit the source after object emission and still require
the original snapshot. Unexecuted branches and and/or expressions remain safe.

### Commands and local verification

Both installed and complete official SDKs were checked for LLJIT.h,
libLLVMOrcJIT.a and LLVMConfig.cmake. Both configuration link/run probes passed.
Installed SDK: `/usr/lib/llvm-19/lib/cmake/llvm`. Complete archive:
`.toolchains/ci-investigation/LLVM-19.1.7-Linux-X64`, using its own matching
Clang/LLD and RTTI settings. Required dependency and platform setup remains intact.

```sh
cmake --build build/ci-debug -j3
ctest --test-dir build/ci-debug --output-on-failure
cmake --build build/ci-release -j3
ctest --test-dir build/ci-release --output-on-failure
cmake --build build/ci-sanitizers -j3
ctest --test-dir build/ci-sanitizers --output-on-failure
ctest --test-dir build/ci-sanitizers --output-on-failure \
  -R '^(frontend|modules|modules_cli|reload|gc|native)$' --repeat until-fail:5
cmake --build build/ci-official-debug -j3
# Final verification uses serial linking after the observation below.
cmake --build build/ci-official-debug -j1
ctest --test-dir build/ci-official-debug --output-on-failure
```

Final installed-SDK Debug: 10/10 passed (28.07 s while SDK builds ran).
Final Release: 10/10 passed (10.87 s). Final ASan/UBSan: 10/10 passed (25.77 s).
ASan, UBSan including both plain/checked entry ABI function checks, and leak detection
remain enabled. Sanitizer CTest runs outside the desktop tracing sandbox for
LeakSanitizer process inspection; generated application instructions and LLVM
itself are not sanitizer-instrumented. Earlier intermediate runs also passed;
a new module diagnostic assertion initially expected alias column 23 instead of
the actual 18 and was corrected to the source position.

Final repeated ASan/UBSan stress: all six selected suites passed five consecutive
runs each, 30 suite executions (192.58 s, concurrent official LLVM bitcode linking).
An intermediate concurrent official-SDK LLD link emitted a Machine Instruction
Scheduler backtrace in LLVMGlobalISel despite reporting build exit status 0.
A final serial build with the same pinned SDK completed cleanly, without another
backtrace. Its cause remains unconfirmed; compare the next hosted Linux link. No libraries, LLVM version or build requirements were substituted.
After final diagnostic wording corrections, Debug again passed 10/10 (17.67 s),
Release 10/10 (16.41 s), and ASan/UBSan 10/10 (27.01 s), with concurrent SDK linking.
Retained logs contain no AddressSanitizer, UndefinedBehaviorSanitizer, LeakSanitizer
or runtime-error warning lines. Final complete official SDK Debug: 10/10 passed (9.58 s), including all
architectural regressions, the numeric oracle and normal/O2 native diagnostics.
Its final serial build and retained test log contain no linker backtrace or
sanitizer reports. Whitespace and local documentation-link checks passed.

### Hosted verification and next phase

This phase needs a new user-published GitHub Actions run: Windows/macOS Debug and
Release, hosted Linux Debug/Release/ASan/UBSan, especially checked source entry ABI,
CRT diagnostic printing, optimized propagation/call traces and the full numeric
oracle. No current hosted success is claimed. CI still uses the complete pinned
19.1.7 SDK and preserved Linux LLD, Windows SDK/DIA/legacy stdio and macOS settings.

Next improve structured diagnostics/recovery and precise source dependency
compatibility, then approve contextual literal/conversion tables before wider
numeric types. Managed types remain behind the separate GC root/thread gate.

## Phase 5: Structured diagnostics and compiler recovery, 10–11 October 2026

### Baseline and scope

The user reports Phase 4 commit `d78b6b8`, GitHub Actions run #6, passed Linux,
Windows and macOS. Read the current Bible/Strategy and source/module/value,
decision, issue and work-log documentation. The Bible's unified-diagnostic IDs
are explicitly illustrative; Phase 5 now assigns 34 stable compiler categories.
No language grammar, LLVM version, runtime failure ABI, const restart contract,
workflow requirements or existing architectural assertions were relaxed.
All work remains local: no branch, commit, push, publication or remote change.

### Implemented and audited

- One Diagnostic/SourceSpan/RelatedLocation representation with severity, assigned
  codes, description, owned original snapshot, physical path/VFS owner, half-open
  byte ranges, related declarations, expected/received types and hints.
  CompilationError preserves invalid_argument compatibility and exposes structured
  records. SourceFile copies now share immutable bytes/line indexes.
- Numbered compiler snippets, scalar-counted ranges, related source snippets and
  code/type/hint rendering. Runtime excerpts and generated checked-entry ABI remain
  unchanged. DIAGNOSTICS.md documents all assigned codes, byte/column rules and
  display-width/multiline/long-line limits.
- Lexer line recovery and indentation-aware parser statement/declaration recovery,
  permanent EOF sentinel and explicit progress/recursion/size guards. Malformed
  named declarations leave invalid bindings rather than fabricated values.
- Semantic error accumulation across statements/functions/constants/modules,
  AST-only poisoned types, failed constant states and poisoned duplicate names.
  Failed imports continue unrelated discovery; uncertain dependent lookups are
  conservatively suppressed. Stable VFS/span/code ordering, equivalent-record
  deduplication, 100-record budget plus a notice and bounded registry/analysis.
- A complete validation gate before AST-to-IR lowering. Invalid check/run/emit
  returns 1 with no native execution, new object or overwritten existing output.
  Valid source still uses the original IR and shared LLVM implementation.
- Failed edits retain active/leased native generations. Exact conservative const
  tokens are unchanged; changed valid const generations still require restart.
  Tests verify an old native division failure retains its original source line
  after an invalid edit, and compile-time snapshots survive file changes/deletion.

Development checks caught and corrected lexer-line recovery producing a secondary
return mismatch, EOF recovery producing a secondary missing-return error, and a
malformed entry UTF-8 snapshot being confused with an empty file-opening anchor.
Snapshot interning now checks that the bytes agree before substituting an owner.
A recursion audit added one semantic depth budget shared across constant analysis,
preventing nested expressions from multiplying separate per-constant limits.
Duplicate declarations/parameters no longer select an arbitrary type for dependent
uses. Failed local calls and implicit alias conflicts preserve poison state, and
related declaration notes are deduplicated even through repeated imports. A recursion fixture originally contained postfix `--` tokens; it now uses
spaced unary minus to test the actual recursive grammar. No old assertion was removed.

### Acceptance and executable evidence

Checked-in real fixtures under `tests/fixtures/diagnostics/` cover eight independent
primary errors in one file, malformed statements between functions and errors in
multiple imported files, including a cross-file previous declaration. Generated
real `.void` fixtures cover malformed signatures/expressions/if-else/for/use,
indentation/tabs, EOF, UTF-8/BOM/LF/CRLF/CR/no-final-newline, spaces in paths,
long lines, scalar ranges, repeated imports, alias conflicts, duplicate-name
poisoning, constant cycles, 100-diagnostic exhaustion, deeply nested/large
expressions, combined expression/constant depth and a 130-file import chain.
Fifteen deterministic multi-file repeats and three repeats of every malformed
fragment assert stable order, termination and retention of later independent errors.

`source_cli` checks invalid check/run/emit in both normal and O2 modes, verifies
empty success output and failure status, compares existing output SHA-256 before/
after failed emission, and requires that no fresh partial object appears. It then
compiles corrected source through both JIT and linked AOT in both modes. Existing
architecture, numeric oracle, runtime propagation, GC, reload and real native
printf proofs remain in the original ten suites.

Actual final demonstration:

```sh
build/ci-debug/voidc check examples/diagnostics/errors.void
# exit 1; all three in one invocation:
# errors.void:2:23 VOID2001 (bool/i32 mismatch)
# errors.void:4:5  VOID2007 (readonly bonus mutation)
# errors.void:5:12 VOID2003 (unknown missing function)
build/ci-debug/voidc run examples/diagnostics/corrected.void 10 --no-opt
# 52
build/ci-debug/voidc run examples/diagnostics/corrected.void 10
# 52
build/ci-debug/voidc emit examples/diagnostics/corrected.void -o build/ci-debug/diagnostic-demo.o
clang-19 --ld-path=/usr/lib/llvm-19/bin/ld.lld build/ci-debug/diagnostic-demo.o -o build/ci-debug/diagnostic-demo
build/ci-debug/diagnostic-demo 10
# 52
```

The multi-file check reports `/a.void` shadowing/unknown identifier, `/b.void`
return mismatch and `/main.void` missing import, with the original root declaration
as a related location. The failed qualifier adds no downstream unknown-call noise.
DIAGNOSTICS.md includes actual formatted snippets and the correction example.
CLI behavior remains: source/runtime failures 1, usage errors 2; invalid JIT input
retains its existing status 1, while the emitted native adapter rejects bad input
with status 2. Verified these directly; this phase does not change those exits.

An additional deterministic mutation run (seed 50019) wrote 300 malformed real
files, each with a later independent unknown identifier. Every compiler process
terminated within its three-second per-process timeout, returned 1, retained that
later diagnostic and exposed no LLVM errors. This is local robustness evidence,
not a claim of exhaustive fuzz coverage. Its log is retained with final build logs.

### Commands, exact toolchains and measured final results

Verified LLJIT.h, libLLVMOrcJIT.a and LLVMConfig.cmake in both the installed
`/usr/lib/llvm-19` SDK and complete official
`.toolchains/ci-investigation/LLVM-19.1.7-Linux-X64` SDK. Existing CMake configurations
retain exact 19.1.7, matching Clang/LLD and their SDK RTTI settings. No dependencies
or probes were removed. The official archive build links serially, preserving the
previous phase's unexplained concurrent-link observation for reference.

```sh
cmake --build build/ci-debug -j2
ctest --test-dir build/ci-debug --output-on-failure
cmake --build build/ci-release -j2
ctest --test-dir build/ci-release --output-on-failure
cmake --build build/ci-sanitizers -j2
env ASAN_OPTIONS=detect_leaks=1:halt_on_error=1:exitcode=87 \
  UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1:exitcode=86 \
  ctest --test-dir build/ci-sanitizers --output-on-failure
env ASAN_OPTIONS=detect_leaks=1:halt_on_error=1:exitcode=87 \
  UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1:exitcode=86 \
  ctest --test-dir build/ci-sanitizers --output-on-failure \
  -R '^(frontend|modules|modules_cli|reload|gc|native)$' --repeat until-fail:5
cmake --build build/ci-official-debug -j1
ctest --test-dir build/ci-official-debug --output-on-failure
```

| Final configuration | Result | CTest wall time |
|---|---|---|
| Installed LLVM 19.1.7 Debug | 10/10 passed | 11.85 s |
| Installed LLVM 19.1.7 Release | 10/10 passed | 14.16 s |
| Installed LLVM 19.1.7 ASan/UBSan | 10/10 passed | 19.41 s |
| Complete official LLVM 19.1.7 SDK Debug, serial build | 10/10 passed | 9.82 s |
| ASan/UBSan repeated stress | Six suites × five runs, all 30 passed | 82.70 s |

Full-build timings vary with concurrent local SDK linking; these are observations,
not performance benchmarks. Checked object timestamps against final source and
acceptance-test timestamps in all four directories so stale intermediate builds
are not counted as final verification. Sanitizer execution runs outside the desktop
tracing restriction for LeakSanitizer inspection; leak detection, UBSan function
checking and assertions remain enabled. Linux CI and final local sanitizer runs
use halt-on-error with distinct ASan/UBSan exit codes 87/86, so an ordinary expected
source-failure exit 1 cannot hide a sanitizer failure in a child compiler process. Generated native instructions and LLVM
itself are not sanitizer-instrumented. Final logs were scanned for sanitizer
reports and LLVM backtraces, in addition to checking CTest status. No such reports
were found. Unique-ID/catalog consistency, local Markdown links and whitespace
checks passed. Final local logs are retained under `build/phase5-verification/`.

### Remaining limitations and hosted verification

DIAGNOSTICS.md records conservative suppression for failed dependencies, recovery
boundaries and resource limits. Lexically unreadable declarations may still cause
later unresolved-name messages. Terminal display width, UTF-16 protocol conversion,
successful-analysis warning/result APIs, incremental parsing/analysis and an LSP
remain future work. Runtime source failure behavior and conservative const restart
compatibility are preserved, not replaced with an inferred dependency hash.

The current workflow automatically runs these expanded ten suites on Linux,
Windows and macOS, including Linux ASan/UBSan. A new user-published Actions run is
still required for Phase 5: hosted Debug/Release on all platforms and hosted Linux
sanitizers, especially filesystem recovery, related ranges, CRT/native output
protection and retained snapshots. No current hosted success is claimed and no
remote operation was performed.

Recommend the next cohesive phase design precise validated source/dependency
compatibility and a reusable analysis-result API before incremental IDE integration.
Keep one parser/type checker. Numeric contextual literals/conversions still require
deliberate policy approval; managed types still require the GC/root/thread gate.

## Phase 6: numeric types and contextual literals, 11 October 2026

The user confirmed Phase 5 commit `4f3688d` passed hosted run #8 across Linux,
Windows and macOS, including the SDK RTTI correction. Read the current Bible
sections 6/30, VALUE_SEMANTICS, DIAGNOSTICS, DECISIONS, KNOWN_ISSUES,
CI_SANITIZER_ABI and DEVELOPMENT_STRATEGY. Presented the requested numeric policy
table and paused before compiler changes. The user explicitly approved the full
ten-type contract, then authorized implementation and complete local verification.

### Implemented contract and shared pipeline

All i8/i16/i32/i64/u8/u16/u32/u64/f32/f64 types now work throughout the existing
source compiler. Unsuffixed literals receive expected context; named/typed values
retain exact types. Integers wrap at their width and preserve the prior i32
MIN/-1 and zero-divisor behavior. Decimal literals construct their selected IEEE
precision directly; integral literal adaptation to float must be exact. Explicit
numeric as permits the approved rounding/truncation with runtime range/non-finite
checks. No bool promotions, as?, managed types or public ABI expansion were added.
NUMERIC_TYPES.md records the full approved contract and remaining syntax limits;
LANGUAGE_BIBLE.md was not edited.

A private LLVM Support APInt/APFloat kernel supplies reliable magnitude parsing
and deterministic declared-precision constant arithmetic/conversion. This is a
narrow new implementation dependency of void_core, with no LLVM types in public
frontend/IR APIs and no second AST evaluator/code generator. IR constants carry
64-bit scalar patterns; conversion/negation are explicit operations. Shared legal
operation helpers serve semantic analysis and IR validation. AOT and ORC still
use one lowering and optional O2 pipeline.

Checked casts guard exact integer ranges and ordered floating bounds before
FPToSI/UI. Float-to-integer lower bounds account for truncation and source
precision, including MIN-1 rounding to MIN at large widths. This removed an
initial trunc/truncf intrinsic approach that would have required native math
library linkage. MIN/-1 division selects a safe divisor before executing the
LLVM operation. Floating negation uses FNeg to preserve signed zero. A boundary
audit added directly signed integer zero in floating context; it now agrees with
floating unary negation, while conversion of an existing integer zero yields +0.
The initial new magnitude parser sign-extension bug was caught by unchanged i32
regressions and corrected before final verification.

### Acceptance coverage and independent calculations

Keep all ten original suite names and architectural assertions. Two negative
frontend fixtures now reject malformed `1.5q` and unsupported string types,
instead of rejecting the newly approved float syntax/types. Add numeric and
numeric_cli: 64 real top-level VOID programs, plus their imported helper.
The numeric suite performs 192 normal/O2 native JIT invocations; numeric_cli
performs 384 normal/O2 JIT/native AOT invocations and 128 actual object/link steps.
Their expressions additionally check compile-time results against explicit
independent expected values. Wider functions are called from source through the
unchanged i32 CLI/host adapter.

Coverage includes 96 signed/unsigned boundary arithmetic cases, all 100 numeric
conversion pairs, 64 valid float-to-integer boundary cases, both invalid boundary
sides for every integer destination in both floating precisions, integer zero
failures, cast failures and caller traces, exact literal adaptation, f32/f64
rounding differences, NaN comparison, signed zero, subnormals, finite overflow,
underflow, non-fused multiply/subtract, per-operation rounding, imports and numeric
constant restart tokens. Source errors preserve snapshots/spans, collect multiple
independent failures, and never return invalid IR. Numeric type context probing
is tested not to hide independent unknown calls in sibling operands. Same-width
incompatible entry signatures are rejected; failed edits preserve the old lease.

The developer-only `tests/numeric/generate_fixtures.py` uses independent Python
unlimited-precision integer formulas and explicit IEEE neighbors, importing no
compiler arithmetic. Regeneration matched every checked-in fixture byte.
The runtime-dependent dynamic_widths program returns 47, -7, 48 for inputs
0, 1, 10; its optimized arithmetic cannot be replaced by a single known result.
Separate handwritten floating fixtures check IEEE edge behavior. No test suite
or sanitizer assertion was disabled.

### Actual source-to-native demonstration

```sh
build/ci-debug/voidc check examples/numeric_types/main.void
build/ci-debug/voidc run examples/numeric_types/main.void 10
# 52
build/ci-debug/voidc emit examples/numeric_types/main.void -o build/phase6-verification/numeric-types.o
clang-19 --ld-path=/usr/lib/llvm-19/bin/ld.lld build/phase6-verification/numeric-types.o -o build/phase6-verification/numeric-types
build/phase6-verification/numeric-types 10
# 52
```

The imported example uses i64 arithmetic, contextual f32/f64 parameters, u64
wrapping and a checked result conversion. A recorded demonstration additionally
verified architecture inputs -1/0/1/10 -> 7/42/43/52, numeric example inputs
0/1/10 -> 42/43/52, and dynamic_widths inputs 0/1/10 -> 47/-7/48 in normal/O2
JIT and linked normal/O2 AOT. Logs and JSON are in build/phase6-verification.
These are parsed files, not reconstructed proof IR.

### Final local commands and measured results

Both SDKs' LLJIT/APFloat headers, OrcJIT library and LLVMConfig.cmake were checked,
and matching Clang 19.1.7 was executed. The installed SDK is `/usr/lib/llvm-19`;
the complete official SDK is `.toolchains/ci-investigation/LLVM-19.1.7-Linux-X64`.
All six build directories retain their matching Clang/LLD, exact SDK and original
build-type/sanitizer settings. Every C++ compile command was checked for SDK-matched
RTTI; sanitizer commands retain `-fsanitize=address,undefined`. No toolchain
requirements, exported dependencies, test assertions or sanitizer flags were removed.

```sh
cmake --build build/ci-debug -j2
ctest --test-dir build/ci-debug --output-on-failure
cmake --build build/ci-release -j2
ctest --test-dir build/ci-release --output-on-failure
cmake --build build/ci-sanitizers -j2
env ASAN_OPTIONS=detect_leaks=1:halt_on_error=1:exitcode=87 \
  UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1:exitcode=86 \
  ctest --test-dir build/ci-sanitizers --output-on-failure
env ASAN_OPTIONS=detect_leaks=1:halt_on_error=1:exitcode=87 \
  UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1:exitcode=86 \
  ctest --test-dir build/ci-sanitizers --output-on-failure \
  -R '^(numeric|numeric_cli|frontend|reload|gc|native)$' --repeat until-fail:3
```

Official archive members contain ThinLTO summaries. Final local SDK configurations
use LLD's standard content-keyed ThinLTO cache to reuse expensive SDK compilation.
This is only a local link-cache setting, not a replacement SDK, changed ABI,
optimization policy or test result cache. Native programs are emitted, linked and
executed afresh. Existing directories were reconfigured as follows (their explicit
SDK/compiler paths remain the ones recorded in CI_SANITIZER_ABI.md):

```sh
cmake -S . -B build/ci-official-release \
  -DCMAKE_EXE_LINKER_FLAGS=-Wl,--thinlto-cache-dir="$PWD/build/phase6-verification/lto-cache"
cmake --build build/ci-official-release -j1
ctest --test-dir build/ci-official-release --output-on-failure
cmake -S . -B build/ci-official-debug \
  -DCMAKE_EXE_LINKER_FLAGS=-Wl,--thinlto-cache-dir="$PWD/build/phase6-verification/lto-cache"
cmake --build build/ci-official-debug -j1
ctest --test-dir build/ci-official-debug --output-on-failure
cmake -S . -B build/ci-official-san \
  -DCMAKE_EXE_LINKER_FLAGS=-Wl,--thinlto-cache-dir="$PWD/build/phase6-verification/lto-cache"
cmake --build build/ci-official-san -j1
env ASAN_OPTIONS=detect_leaks=1:halt_on_error=1:exitcode=87 \
  UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1:exitcode=86 \
  ctest --test-dir build/ci-official-san --output-on-failure
env ASAN_OPTIONS=detect_leaks=1:halt_on_error=1:exitcode=87 \
  UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1:exitcode=86 \
  ctest --test-dir build/ci-official-san --output-on-failure --repeat until-fail:3
```

| Final configuration | Result | CTest wall time |
|---|---|---|
| Installed LLVM 19.1.7 Debug | 12/12 passed | 39.59 s |
| Installed LLVM 19.1.7 Release | 12/12 passed | 30.07 s |
| Installed LLVM 19.1.7 ASan/UBSan, RTTI enabled | 12/12 passed | 75.04 s |
| Complete official LLVM 19.1.7 Debug | 12/12 passed | 26.69 s |
| Complete official LLVM 19.1.7 Release | 12/12 passed | 24.33 s |
| Complete official LLVM 19.1.7 ASan/UBSan, RTTI disabled | 12/12 passed | 47.89 s |
| Installed sanitizer repeats | Six suites × three, all 18 passed | 162.60 s |
| Official sanitizer repeats | Twelve suites × three, all 36 passed | 144.88 s |

Timings reflect local concurrent work and cache state; they are observations,
not compiler-performance benchmarks. LeakSanitizer runs outside the desktop
tracing restriction. The installed SDK retains vptr coverage; the official no-RTTI
SDK cannot provide that check. Both retain supported UBSan checks, ASan, leak
detection, fatal report handling and the existing UBSan JIT entry type metadata.
LLVM itself and generated application instructions remain uninstrumented, as
explicitly documented before this phase. No sanitizer coverage equivalence is claimed.

Re-executed the seven existing standalone sanitizer fault controls: heap overflow,
signed overflow and wrong indirect-call type in both RTTI modes, plus invalid
vptr in RTTI mode. All emitted the expected report and exited 86 under the strict
combined runtime settings. These reuse the previously compiled control binaries,
not intentionally faulty VOID programs. The shared sanitizer runtime's exit behavior
remains the one documented in CI_SANITIZER_ABI.md.

Additional final recovery stress used seed 60019: 300 malformed real numeric files,
including excessive cast chains, each retained a later independent missing-name
error, exited 1 within a three-second timeout, and exposed no raw LLVM diagnostic.
New as nodes count toward the existing expression-size guard. Unknown callees
still permit independent argument diagnostics; nested constant failures preserve
their original structured code/span rather than being rewrapped at a parent operator.

Final source/fixture hashes and relevant object timestamps were checked in all six
directories, so stale intermediate builds are not counted. Final logs were scanned
for sanitizer reports, LLVM errors/backtraces and project warnings; none were found.
Only the already documented GCC 16/LLVM ciso646 deprecation warning remains.
Local Markdown links and git diff whitespace checks passed. Evidence is retained
under ignored `build/phase6-verification/`: final configuration/build/test/repeat
logs, toolchain information, source manifest, independent oracle/regeneration,
recovery cases, cast failure trace and source/native demonstration.

### Remaining limitations and next phase

NUMERIC_TYPES.md documents deferred exponent/base/separator literal spellings,
float remainder, nullable as?, platform-sized aliases, numeric hashing/formatting,
foreign floating-control-state management and arbitrary public native entry ABIs.
The current CLI remains i32(i32), bool remains separate, and no managed/runtime
systems were expanded. No numeric family members are deferred.

The existing workflow automatically includes both new suites on Linux, Windows
and macOS. Phase 6 still requires a new user-published Actions run for actual
hosted Debug/Release on all platforms and Linux sanitizers. Local Linux results
cannot substitute for Windows/macOS executable verification. No hosted Phase 6
success is claimed. No commit, push, branch change, publication or remote mutation
was performed. Recommended next cohesive phase: validated analysis/dependency
APIs and compatibility metadata before incremental tooling; managed types still
require their separate GC root/thread integration gate.

## Phase 7: validated analysis and dependency compatibility — 11 October 2026

The user reports Phase 6 commit `9b9fdac` passed Actions run #9 on Ubuntu,
Windows and macOS. This phase proceeded locally from that clean tree. No branch,
commit, push, publication or remote mutation was performed. LLVM/Clang/LLD remain
19.1.7; the shared SDK RTTI policy, numerical semantics, twelve existing CTest
suites and architectural assertions remain intact. The Language Bible is unchanged.

### Implementation and safety decisions

Reviewed the current frontend, source graph, semantic/constant evaluator, lowering,
IR, JIT publication contract and project documentation. Split the existing graph
pipeline at its complete validation gate. `analyze_source`/`analyze_project` return
explicit success with an immutable owning ValidatedAnalysis, or diagnostics with
no program. Existing compilation adapters analyze once and lower the retained
validated tree. Analysis itself generates neither VOID IR nor native code.

The result owns canonical VFS modules and immutable source snapshots, typed/visible
function/module/local-constant declarations, source spans, module constant bit
patterns, containing owners and real resolved call/constant references. Module
constants retain their declaring identity when semantics substitutes literal bits.
Local readonly references use typed slot ownership to identify the declaration.
Function-only import cycles require no acyclic assumption or duplicate analysis.

Exact tagged length-delimited comparison data now covers the graph, declarations,
types/visibility and resolved references, plus parser-captured constant tokens.
Trivia, physical paths and source offsets are excluded. Constants include unused,
root, imported, transitive and runtime local readonly declarations. Equal evaluated
values do not hide changed declaration syntax. No finite hash authorizes reload.
Local constant IDs use function/name/lexical same-name occurrence, not raw lines.

Added validated-source JIT compilation and preflight replacement. Compiler-owned
source records override caller-supplied source compatibility fields; host layout,
capture and legacy contracts are additional requirements. Incompatible candidates
return false before native generation. Slot publication rechecks compatibility
under the existing mutex, and old code retains its resource tracker and source
analysis through leases. No automatic restart, watcher, incremental cache, LSP or
unproven multi-slot transaction was introduced. Legacy hand-built IR proofs remain
available; manually constructed IR/native code is a trusted host interface.

An initial focused run correctly exposed a preserved architectural requirement:
unused alias renaming across physical relocation must remain compatible. Canonical
imports now compare actual VFS targets and references compare resolved owners,
while tooling records still retain written spellings. The original module test
then passed unchanged. This was a correction to the new comparison, not a relaxed
architectural assertion. The preliminary failure is not included in final pass claims.

Conservative restrictions remain: declaration order and reference traversal order/
multiplicity participate, so some declaration moves or added calls require restart.
The new convenience replacement helper selects the project entry. Arbitrary host
entries still use the legacy IR interface and retain exact i32(i32) checking.
All ten numeric source types remain distinct in declaration contracts even when
LLVM uses equal storage widths. ANALYSIS_COMPATIBILITY.md documents the full API,
ownership model, classification and tooling boundaries.

### New source tests and observed native behavior

Added `analysis` and `analysis_cli`; all twelve prior suites remain, total fourteen.
The real three-file `examples/analysis/` includes project-root/module/local consts.
Normal and optimized JIT both observed:

```text
input 10, initial source: 53
edit only return stable + 1 -> return stable + 9: compatible, new slot returns 61
old native lease: 53
edit imported const base = 40 -> 41: incompatible, slot remains 61
```

The incompatible preflight does not allocate another native generation. A second
path deliberately compiles the incompatible candidate and proves direct slot
replacement still rejects it even when host metadata attempts to copy old tokens.
Comment/blank-line/constant-spacing edits remain compatible, updating spans without
changing symbol identities. Retained analysis lowers and executes after deletion of
all source files; old runtime failures still show their original source line.

Other focused coverage includes module/local/transitive constants, unchanged-value
constant syntax edits, visibility/signature changes, declaration addition/removal,
module graph changes, imported rename/rebinding, distinct same-named owners, cycles,
relocation, all ten numeric types, host layout/capture guards and multiple structured
errors. Each analysis run executes 480 malformed-source cases across the two JIT
modes, checking recovery retains a following independent diagnostic. Concurrent
callers acquire leases during 24 actual compatible native publications per suite
run; returned results must belong entirely to one generation. Existing deterministic
native-call retirement/GC/C-binding proofs also execute unchanged.

A recorded verbose Debug demonstration observed 229,310 normal-mode calls and
266,992 O2-mode calls during the source publication exercise; these are correctness
stress counts, not performance measurements. `analysis_cli` independently checks
53 -> 61 -> 62 through fresh real-source normal/O2 JIT and linked native AOT.
The final constant edit is legal in a fresh executable; live publication remains
forbidden. Six real object files are linked in each analysis_cli run.

### Actual local validation

Reused six established build directories, triggering the unchanged actual ORC
configure/link/execute SDK probe after CMake changed. SDK headers, APFloat header,
OrcJIT/Support libraries, LLVMConfig.cmake and Clang version were checked for each
configuration; metadata is in `build/phase7-verification/toolchains.json`.

```sh
cmake --build build/ci-debug -j2
ctest --test-dir build/ci-debug --output-on-failure
cmake --build build/ci-release -j2
ctest --test-dir build/ci-release --output-on-failure
cmake --build build/ci-sanitizers -j2
env ASAN_OPTIONS=detect_leaks=1:halt_on_error=1:exitcode=87 UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1:exitcode=86 ctest --test-dir build/ci-sanitizers --output-on-failure
cmake --build build/ci-official-debug -j1
ctest --test-dir build/ci-official-debug --output-on-failure
cmake --build build/ci-official-release -j1
ctest --test-dir build/ci-official-release --output-on-failure
cmake --build build/ci-official-san -j1
env ASAN_OPTIONS=detect_leaks=1:halt_on_error=1:exitcode=87 UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1:exitcode=86 ctest --test-dir build/ci-official-san --output-on-failure
```

Installed SDK: `/usr/lib/llvm-19/lib/cmake/llvm`, matching installed Clang/LLD19.1.7,
RTTI on. Complete official SDK: `.toolchains/ci-investigation/LLVM-19.1.7-Linux-X64`,
its matching tools, RTTI off. The official builds retain the existing local ThinLTO
cache under `build/phase6-verification/lto-cache`; no SDK files are changed.
Strict sanitizer tests execute outside the tracing sandbox so LeakSanitizer process
inspection works, retaining leak detection and halt-on-error. The official no-RTTI
ABI cannot check vptr; the separate installed RTTI-enabled SDK supplies that coverage.

| Configuration | Actual result | CTest wall time |
|---|---|---|
| Installed Debug | 14/14 passed | 39.45 s |
| Installed Release | 14/14 passed | 32.01 s |
| Installed strict ASan/UBSan | 14/14 passed | 60.52 s |
| Official Debug | 14/14 passed | 28.53 s |
| Official Release | 14/14 passed | 25.22 s |
| Official strict ASan/UBSan | 14/14 passed | 49.71 s |

Additional installed strict sanitizer stress ran `analysis`, `reload`, `modules`
with `--repeat until-fail:5`: all fifteen executions passed, 13.09 seconds.
The normal test output's three named suites represents fifteen actual repeats.
The complete official SDK ran the same three suites five times under strict
sanitizers: all fifteen executions passed in 11.77 seconds. Together these are
thirty additional compatibility/reload/module stress executions, including 4,800
malformed analysis inputs in the ten repeated analysis suite executions.

The complete official strict ASan/UBSan run passed all fourteen suites, including
the original native/GC/generation proofs and Phase 5/6 diagnostics/numerics. The
final audit verified all six compiler/test binaries are newer than the frozen
source/fixture inputs, the SHA-256 source manifest is unchanged, and all six
complete test logs contain 14/14 passes. Final build/test logs contain no sanitizer
reports, LLVM backtraces or new project warnings; only the established LLVM/GCC16
third-party `<ciso646>` warning remains. Markdown links and diff whitespace pass.
Both installed and official LLD report 19.1.7. No SDK or workflow requirements
were changed. The manifest is an audit artifact, not reload authorization.
Logs and source manifests are local ignored artifacts under
`build/phase7-verification/`. Windows/macOS Phase 7 hosted execution requires the
user's next push. The workflow already runs every CTest suite; no platform skips,
sanitizer suppressions or weakened requirements were added. Next recommended work:
an in-process tooling consumer and explicit invalidation planning before incremental
compilation/LSP transport; keep the separate managed-runtime integration gate.

## Phase 8: in-process tooling foundations — 11 October 2026

The user reports Phase 7 commit `04768bf` passed hosted Actions run #10 on Linux,
Windows and macOS. Continued directly from that clean local tree. Reviewed the
actual frontend records, validation/lowering boundary, native comparison/publication
API and current analysis, diagnostics, module, strategy, decision and issue records.
The Language Bible and Phase 7 compiler-derived compatibility machinery are unchanged.
No branch, commit, push, publication or remote modification was performed.

### Implementation

Added `include/void/tooling.hpp` and `src/frontend/tooling.cpp` as a small consumer
of the successful immutable analysis, linked into void_core. ToolingIndex retains
shared ownership and indexes stable declaration identities, declarations by module,
incoming resolved references and per-module byte positions. Queries return copied
structured records with owning SourceSpans. Optional/empty results represent absent
or unsupported symbols. No AST, token or declaration pointers escape. No parsing,
semantic checking, LLVM code generation, symbol scanning or global registry occurs.
Canonical declaring ownership survives aliases and root-constant references. Types,
visibility, function signatures, constant values and spans come directly from the
compiler. Only the compiler-collected function/module/local-readonly categories
are supported; mutable locals, parameters, import qualifier tokens, member completion
and arbitrary expression hover are not invented. UTF-8 offsets remain byte offsets.

Added a deterministic invalidation planner over reverse imports and resolved
reference edges, with explicit local-constant-to-containing-function dependencies.
Finite iterative traversal handles cycles. An old graph plus changed VFS hints
requires fresh complete graph discovery and validation. Namespace changes include
reverse importers; unknown module names and unknown root edits conservatively
include the whole known graph. Missing/invalid candidates expose no successful
comparison. New/removed modules are confirmed only between successful graphs.

Two successful snapshots provide actual source-byte differences, added/removed
reachable modules, declaration/constant metadata differences, changed resolved
bindings and exact source-contract classification. Both dependency graphs are
traversed as a union, preserving old dependents after deletion/rebinding. Physical
relocation does not count as a source edit. Changed files still seed every contained
declaration because Phase 7 does not collect function-body extents; no hidden second
parser is used to pretend otherwise. These are potential reanalysis sets, not a
partial compiler or a proof that a body/comment delta was identified exactly.

The planner compares the authoritative Phase 7 compiler contract verbatim. It
cannot publish code, substitute metadata summaries for that comparison, override
host ABI/layout/capture contracts or certify that snapshots match later disk edits.
The public flag is `has_validated_comparison`, deliberately not a claim of current
filesystem freshness. Work-required flags refer to unresolved work between the
supplied snapshots. Unknown hinted identities restore conservative unknown status.

### Real-source tests and executable example

Added `tooling` and `tooling_demo`, retaining all fourteen previous CTest suites and
assertions, total sixteen. Real filesystem projects test direct/aliased definitions,
multiple callers, same-named declarations, root and readonly local constants,
typed/visible records, exact identifier byte ranges, a UTF-8 comment containing
an accented scalar and emoji, changed line positions, forward references, cycles,
transitive invalidation, imported constants, signature/visibility changes, rebinding,
added/removed/renamed/missing/invalid modules and duplicate normalized imports.

Owned records survive original analysis/index destruction, source file deletion,
new analysis creation and native replacement. An independent u64 constant result
outlives both its index and analysis with the full 64-bit value intact. An unused
local const initializer still invalidates its containing function. Repeated queries
and plans compare deterministically, including equivalent physical relocations;
cycles perform 100 traversals per suite and relocation queries 30 comparisons.
The actual source JIT proof remains 53 -> 61, while the old lease stays 53 and
constant/signature changes cannot publish. No architectural regression was replaced
with a mock. Normal/O2 native AOT/JIT and numeric parity remain exercised by all
fourteen existing suites, including the Phase 7 normal/O2 publication proof.

`examples/tooling.cpp` builds as `void_tooling_example`, linked only to void_core.
It analyzes the actual three-file analysis example, looks up a declaration and its
resolved references, edits a disposable copy, plans invalidation, completely
reanalyzes, and distinguishes body/constant source contracts. CTest copies its
fixture before each run, so repeated execution never edits checked-in sources.
The example deliberately edits its argument directory; documentation requires a
copied disposable project. Actual recorded output:

```text
/worker.void::calculate declared in /worker.void; references: 1
edit plan: full validation required; fresh body source contract: exact match
fresh constant source contract: restart required; no native generation modified
```

The focused native/tooling test additionally recorded:

```text
queries: aliased/direct definitions, 2 callers, root/local constants, byte spans and retained snapshots
plan: /leaf.void -> /middle.void::wrapper -> /main.void::entry; validated 53 -> 61, const restart required
```

See `build/phase8-verification/demonstration.log`. TOOLING_FOUNDATIONS.md documents
the API, lifetime contracts, supported categories, conservative outcomes and future
LSP/incremental boundaries. Updated decisions, strategy, known issues, README and
the Phase 7 analysis document. No watcher, JSON-RPC, extension, background compiler,
persistent cache or partial validation pipeline was introduced.

### Actual local verification

Reused installed `/usr/lib/llvm-19/lib/cmake/llvm` and complete official
`.toolchains/ci-investigation/LLVM-19.1.7-Linux-X64` SDK configurations. CMake changes
rerun the established actual ORC configure/link/execute probe. RTTI stays on for
installed SDK targets and off throughout official SDK targets. LLVM, Clang and
LLD remain 19.1.7. Official builds use their existing local ThinLTO linker cache;
no SDK files or dependency/build requirements were modified.

```sh
cmake --build build/ci-debug -j2
ctest --test-dir build/ci-debug --output-on-failure
cmake --build build/ci-release -j2
ctest --test-dir build/ci-release --output-on-failure
cmake --build build/ci-sanitizers -j2
env ASAN_OPTIONS=detect_leaks=1:halt_on_error=1:exitcode=87 UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1:exitcode=86 ctest --test-dir build/ci-sanitizers --output-on-failure
cmake --build build/ci-official-debug -j1
ctest --test-dir build/ci-official-debug --output-on-failure
cmake --build build/ci-official-release -j1
ctest --test-dir build/ci-official-release --output-on-failure
cmake --build build/ci-official-san -j1
env ASAN_OPTIONS=detect_leaks=1:halt_on_error=1:exitcode=87 UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1:exitcode=86 ctest --test-dir build/ci-official-san --output-on-failure
```

Strict sanitizer execution runs outside the tracing sandbox so LeakSanitizer
process inspection works. Leak detection and halt-on-report remain enabled. The
no-RTTI official ABI still cannot provide vptr checking; the installed RTTI-enabled
lane retains that coverage. All prior numerical, diagnostics, native bindings, GC
root and leased-generation tests execute unchanged.

| Configuration | Actual result | CTest wall time |
|---|---|---|
| Installed Debug | 16/16 passed | 36.34 s |
| Installed Release | 16/16 passed | 30.91 s |
| Installed strict ASan/UBSan | 16/16 passed | 58.65 s |
| Official Debug | 16/16 passed | 28.03 s |
| Official Release | 16/16 passed | 25.81 s |
| Official strict ASan/UBSan | 16/16 passed | 49.51 s |

Installed strict sanitizer repeats of `tooling`, `tooling_demo`, `analysis`, `reload`
with `--repeat until-fail:5` passed all twenty executions in 10.72 seconds. This
includes actual cycle/relocation queries, source lifetime checks, compiler recovery
and concurrent generation replacement. The same complete-official-SDK strict
sanitizer repeats passed all twenty executions in 9.68 seconds. Together these
are forty additional stress executions, including 1,000 cycle traversals and
4,800 malformed compiler-recovery inputs in the repeated tooling/analysis suites.
All retained source/query/index/native lifetime assertions executed under both
SDK sanitizer ABIs.

Final audit confirms all six compiler/test/example binaries are newer than their
frozen source/fixture inputs, and the SHA-256 manifest is unchanged. Each complete
configuration passed 16/16. Recorded build/test logs contain no sanitizer report,
LLVM backtrace or new project warning. Both LLD tools report 19.1.7; headers,
OrcJIT/Support libraries, LLVMConfig.cmake and Clang versions were checked in all
six configurations. The standalone example's six actual link lines include no
void_llvm, LLVMOrcJIT or void_runtime, confirming its frontend-only boundary.
Markdown links and diff whitespace pass. No SDK, runtime, backend, frontend
validation/compatibility code, sanitizer requirement or workflow was weakened.
The manifest is a verification artifact, never reload authorization. Local logs
and manifests are ignored artifacts in `build/phase8-verification/`. Hosted Phase 8
Windows/Linux/macOS verification requires the user's push; no hosted result for
these changes is claimed. The existing workflow automatically executes all suites.
Next establish explicit document/project revision handling and client invalidation
orchestration before partial compilation or LSP transport. Keep the separate GC/
thread-root integration and external-package identity gates.

## Phase 9 document revisions and tooling sessions, 11 October 2026

The user reports Phase 8 commit `f25b268` passed hosted run #11 on Linux,
Windows and macOS. Read the existing frontend/tooling and relevant documentation;
continued locally without branches, commits, pushes or remote changes.

Added owned ProjectSnapshot/SessionAnalysis and a noncopyable single-threaded
ToolingSession. Logical project identity is a retained unique token, with a numeric
revision for display; per-document nonzero revisions strictly increase even across
close/delete/reopen. Equal bytes do not imply equal revisions. Open, whole-text
replace, explicit unavailable/tombstone, close-to-disk and explicit disk refresh
are distinct transitions. The absolute entry is fixed at session construction.

Selected overlay maps share immutable SourceFile bytes and remain stable through
subsequent edits. Analysis execution and acceptance are separate: delayed, foreign
and duplicate results cannot update current diagnostics, queries, success or plans.
Current queries return absence after edits or failed validation, while explicit
historical handles retain owned records and the last successful baseline.

Extended the existing SourceGraph loader, not the parser/resolver/type checker,
with exact VFS overrides and virtual directory children. Existing path portability,
case, root-escape, symlink/redirect and hard-link restrictions remain. Virtual
file/directory conflicts reject. Tombstones never read old disk bytes. The overlay
analysis overload re-discovers through the same loader and compares exact bytes
and reachable modules before semantic acceptance. Documented its non-atomic disk
boundary, ABA/concurrent mutation limitations and caller refresh responsibility.
Original disk compile_project/CLI/JIT/AOT interfaces remain unchanged.

Session invalidation delegates to Phase 8 plan_invalidation, comparing a successful
candidate against the prior successful baseline before advancing it. Invalid,
stale and unvalidated candidates cannot produce a refined safe classification.
The Phase 7 exact source contract and separate native publication remain intact.

Added sessions and sessions_demo, preserving all sixteen prior suites/assertions.
Tests use real .void filesystem projects and shared analysis: entry/import/multiple
unsaved edits, virtual entry/nested imports, normalized paths/cycles, deletion and
close fallback, exact-case/file-directory collisions, overlay alias restrictions,
strict logical revisions, reordered execution/acceptance, equal bytes/new identity,
foreign session rejection, failed/repair transitions, UTF-8/CRLF byte diagnostics,
retained snapshots/queries, body/constant/signature/binding compatibility and forty
repeated transitions per session suite. Relative entry selection survives working-
directory changes. Historical session analysis lowers through the actual JIT;
existing normal/O2 native source tests retain 53 -> 61 and old lease 53, constant
restart and all architectural numeric/GC/native proofs.

The standalone void_sessions_example links only void_core and consumes the real
examples/analysis project without writing disk. Actual output is retained at
build/phase9-verification/demonstration.log:

```text
revision 1: /worker.void::calculate; references 1
unsaved edit: previous analysis stale; full validation required
fresh query: /worker.void::calculate; body exact match; affected dependencies: /main.void
revision 3 diagnostics:
/worker.void:2:12: error: Unknown identifier 'missing'.
    code: VOID2002
repair: fresh successful analysis
constant edit: restart required; no native publication
```

TOOLING_SESSIONS.md describes public APIs, ownership, revision semantics, freshness,
disk consistency, diagnostic positions, dependency planning and remaining scope.
Updated decisions, strategy, known issues, analysis/module/diagnostic/tooling docs
and README. No LSP, JSON-RPC, watcher, worker, cache, partial AST reuse, incremental
LLVM generation, new language syntax or automatic publication/restart is added.

During verification the existing module regression caught a changed symlink
message; restored the established diagnostic without weakening its assertion.
Review added tests/corrections for fixed relative entry roots and virtual
file/directory conflicts. All final results below use the frozen final sources,
not earlier interim successful binaries.

### Final local validation

Reused the installed `/usr/lib/llvm-19/lib/cmake/llvm` SDK (RTTI on) and complete
`.toolchains/ci-investigation/LLVM-19.1.7-Linux-X64` official archive (RTTI off).
Verified both SDKs' ORC/APFloat headers, Support/OrcJIT libraries, LLVMConfig.cmake,
Clang and LLD 19.1.7. CMake retains its actual ORC configure/link/execute probe.
Official builds are serial and use the existing ThinLTO cache; no SDK components,
requirements, sanitizer settings or RTTI correction were changed.

Actual final commands (existing configured build directories):

```sh
cmake --build build/ci-debug -j2
ctest --test-dir build/ci-debug --output-on-failure
cmake --build build/ci-release -j2
ctest --test-dir build/ci-release --output-on-failure
cmake --build build/ci-sanitizers -j2
env ASAN_OPTIONS=detect_leaks=1:halt_on_error=1:exitcode=87 UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1:exitcode=86 ctest --test-dir build/ci-sanitizers --output-on-failure
cmake --build build/ci-official-debug -j1
ctest --test-dir build/ci-official-debug --output-on-failure
cmake --build build/ci-official-release -j1
ctest --test-dir build/ci-official-release --output-on-failure
cmake --build build/ci-official-san -j1
env ASAN_OPTIONS=detect_leaks=1:halt_on_error=1:exitcode=87 UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1:exitcode=86 ctest --test-dir build/ci-official-san --output-on-failure
```

Strict sanitizers run outside the tracing sandbox, retaining leak detection and
halt-on-error. Installed RTTI-on validation retains vptr coverage; the official
archive's RTTI-off ABI still cannot provide that check. All supported ASan/UBSan
instrumentation remains active. The official build's existing third-party
ciso646 deprecation warning remains visible; no project warning or LLVM backtrace
was found in final logs.

| Configuration | Actual result | CTest wall time |
|---|---|---|
| Installed Debug | 18/18 passed | 38.57 s |
| Installed Release | 18/18 passed | 34.39 s |
| Installed strict ASan/UBSan | 18/18 passed | 57.27 s |
| Official Debug | 18/18 passed | 32.21 s |
| Official Release | 18/18 passed | 26.23 s |
| Official strict ASan/UBSan | 18/18 passed | 49.99 s |


Repeated sessions, sessions_demo, tooling, tooling_demo, analysis and reload ten
times per strict sanitizer SDK (the same leak/halt settings):

```sh
env ASAN_OPTIONS=detect_leaks=1:halt_on_error=1:exitcode=87 UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1:exitcode=86 ctest --test-dir build/ci-sanitizers --output-on-failure -R '^(sessions|sessions_demo|tooling|tooling_demo|analysis|reload)$' --repeat until-fail:10
env ASAN_OPTIONS=detect_leaks=1:halt_on_error=1:exitcode=87 UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1:exitcode=86 ctest --test-dir build/ci-official-san --output-on-failure -R '^(sessions|sessions_demo|tooling|tooling_demo|analysis|reload)$' --repeat until-fail:10
```

Installed: **60/60 executions**, 22.26 s. Official: **60/60 executions**, 19.98 s.
Together the additional 120 executions include 800 repeated session body-edit
transitions, reordered acceptance and snapshot-lifetime cases, and 9,600 malformed
analysis recovery inputs. No sanitizer/leak report occurred.

Final audit verifies all six binaries are newer than the frozen source/fixture
manifest and each full test log is newer than its final binaries. All C++ compile
commands match their SDK RTTI setting; sanitizer compile commands retain ASan/UBSan.
Both standalone examples' six actual link lines exclude void_llvm, void_runtime
and LLVMOrcJIT. Exact SDK/compiler/linker checks, source manifest, build/test/stress
logs, demo output and audit scripts are ignored local artifacts under
build/phase9-verification/. Markdown links and git diff whitespace pass.

Phase 9 is complete locally and ready for hosted CI. Another user-triggered
Linux/Windows/macOS run is required to verify these new filesystem/overlay and
session cases on hosted systems; no new hosted result is claimed. Existing CI
already builds all targets and runs all eighteen suites, with no workflow skips
or requirement changes. The non-atomic disk-read boundary and explicit refresh
obligation remain documented limitations. Next consider protocol position adapters
and a minimal client transport before partial compilation; managed-runtime and
external-package identity gates remain separate. No commit, branch, push,
publication or remote modification was performed.

## Phase 10: minimal language server, 11 October 2026

Baseline: the user reports Phase 9 commit `02a9aa7`, Actions run #12, passed Linux,
Windows and macOS. Implemented this phase locally without branches, commits,
pushes, publication or remote changes. The original eighteen suites and assertions
are unchanged; lsp_positions, lsp_protocol and lsp_demo bring the total to **21**.

### Implementation and evidence

Added a real synchronous, single-threaded `voidls --entry <path>` executable.
It speaks framed stdio JSON-RPC/LSP 3.17: initialize/initialized/shutdown/exit,
full didOpen/didChange/didClose, publishDiagnostics, definition and references.
Only implemented capabilities are advertised. LLVM Support JSON handles parsing
and serialization; framing, lifecycle/errors and resource bounds are explicit.
Numeric/string IDs, negative/zero versions, stale/duplicate rejection, reopening,
multiple full-change events and empty change arrays are exercised through pipes.

The adapter retains exact SourceFile snapshots, reuses the compiler line index
and UTF-8 scalar validator, converts to UTF-16, handles LF/CRLF/CR and rejects split
scalar/surrogate positions. URI conversion handles escaped UTF-8 physical paths,
spaces, `#`, `%`, Windows drive notation and wide argv/binary streams. Another
Windows drive and malformed UTF-8 URI paths reject as parameters. The compiler's
physical-path labels and LLVM debug filenames now use explicit UTF-8 conversion;
portable ASCII VFS module-name restrictions are unchanged. No numeric/syntax,
compatibility, frontend resolver or runtime publication behavior was replaced.

Every accepted transition uses ToolingSession selection/full analysis/acceptance.
Invalid/currently stale graphs have no navigation fallback. Diagnostics preserve
compiler codes, severity, hints, ranges, related locations and negotiated open-file
versions; repair, graph removal and close publish empty arrays where needed.
Navigation consumes existing declarations/resolved references, including imported
functions, root/import/local readonly constants, forward/cyclic calls and multiple
callers. Unsupported mutable locals/parameters/expressions have empty results.

The portable Python harness launches actual subprocesses with binary pipes and
real `.void` fixtures. Tests cover fragmented/coalesced messages, malformed JSON,
invalid IDs, invalid UTF-8, excessive nesting, header/length/truncation errors,
UTF-16 emoji, CRLF/CR, zero-width EOF diagnostics, Unicode physical roots, aliases,
related locations, virtual modules, multi-file unsaved edits, stale/duplicate
versions, invalid/repair, close/reopen, graph changes and clean lifecycle/EOF.
The demo resolves its temporary root (macOS `/var` may alias `/private/var`) so
canonical output URIs compare correctly. No POSIX-only process API is required.

Actual demonstration command/output:

```sh
python3 tools/lsp_demo.py build/ci-debug/voidls
```

```text
initialized: full synchronization, UTF-16, definition and references
open: no diagnostics
definition: worker.void:1:6
references: declaration and one source call
unsaved edit: VOID2002 at worker.void:2:12
invalid revision: navigation returns null
repair: old diagnostics cleared; fresh navigation restored
shutdown and exit: clean protocol streams
```

### Final verification

Reused and regenerated the six existing CMake configurations. Installed SDK:
`/usr/lib/llvm-19/lib/cmake/llvm`, Clang/LLD 19.1.7, RTTI on. Complete official SDK:
`.toolchains/ci-investigation/LLVM-19.1.7-Linux-X64`, its own Clang/LLD 19.1.7,
RTTI off. Verified actual headers (LLJIT/APFloat), static libraries
(LLVMOrcJIT/LLVMSupport), LLVMConfig.cmake and compiler/linker versions. Required
LLVM development dependencies remain intact. Official archive builds ran serially
with the existing ThinLTO cache; the installed builds used two jobs.

```sh
cmake --build build/ci-debug -j2
ctest --test-dir build/ci-debug --output-on-failure
cmake --build build/ci-release -j2
ctest --test-dir build/ci-release --output-on-failure
cmake --build build/ci-sanitizers -j2
env ASAN_OPTIONS=detect_leaks=1:halt_on_error=1:exitcode=87 UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1:exitcode=86 ctest --test-dir build/ci-sanitizers --output-on-failure
cmake --build build/ci-official-debug -j1
ctest --test-dir build/ci-official-debug --output-on-failure
cmake --build build/ci-official-release -j1
ctest --test-dir build/ci-official-release --output-on-failure
cmake --build build/ci-official-san -j1
env ASAN_OPTIONS=detect_leaks=1:halt_on_error=1:exitcode=87 UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1:exitcode=86 ctest --test-dir build/ci-official-san --output-on-failure
```

All full suites were rerun after the final demo/client revision. Final measured
results (Linux host, not new hosted-platform claims):

| Configuration | Result | CTest wall time |
|---|---|---|
| Installed Debug | 21/21 passed | 34.45 s |
| Installed Release | 21/21 passed | 31.18 s |
| Installed strict ASan/UBSan | 21/21 passed | 56.58 s |
| Official Debug | 21/21 passed | 28.58 s |
| Official Release | 21/21 passed | 26.10 s |
| Official strict ASan/UBSan | 21/21 passed | 51.23 s |

Repeated six suites ten times per SDK with the same strict settings:

```sh
env ASAN_OPTIONS=detect_leaks=1:halt_on_error=1:exitcode=87 UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1:exitcode=86 ctest --test-dir build/ci-sanitizers --output-on-failure -R '^(lsp_positions|lsp_protocol|lsp_demo|sessions|analysis|reload)$' --repeat until-fail:10
env ASAN_OPTIONS=detect_leaks=1:halt_on_error=1:exitcode=87 UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1:exitcode=86 ctest --test-dir build/ci-official-san --output-on-failure -R '^(lsp_positions|lsp_protocol|lsp_demo|sessions|analysis|reload)$' --repeat until-fail:10
```

Installed **60/60 executions**, 26.10 s; official **60/60**, 24.51 s.
No sanitizer/leak findings occurred. LeakSanitizer ran outside the tracing sandbox;
no checks or tests were disabled. Installed RTTI-on coverage retains vptr;
the official archive's no-RTTI ABI still cannot provide that check. All supported
ASan/UBSan checks and the previous RTTI correction remain active.

The existing analysis tests still assert native 53 -> 61, an old lease returning
53, constant-edit rejection requiring restart, invalid-source rejection and
concurrent native lease safety in normal/O2 modes. All original GC/native/AOT/JIT
architectural regressions passed. The actual voidls link lines for all six builds
exclude void_llvm, LLVMOrcJIT and void_runtime; LLVMSupport remains linked for JSON
and the existing frontend's numeric support. No second compiler was added.

One initial official Clang attempt crashed in Lexer::SkipWhitespace while its
source file was being rewritten during build preparation. That attempt is retained
in `build/phase10-verification/initial-compiler-interruption.log`. The source was
then frozen before validation. The serial retry, all six final builds and three
additional identical official Clang compilations of the frozen server source
succeeded without a backtrace. Concurrent rewriting is consistent with the lexer
failure; a standalone toolchain defect was not reproduced. Do not treat that initial
attempt as clean or modify sources while the compiler reads them. The unrelated
historical LLD scheduler observation remains documented in KNOWN_ISSUES.md.

Final audit checks frozen source/fixture/client hashes (excluding generated pyc),
relevant binary freshness, full test-log freshness, all C++ RTTI compile flags,
sanitizer instrumentation, exact LLD version and the server's link boundary.
Final build/test logs contain no LLVM backtrace, sanitizer finding or new project
warning. The known third-party ciso646 warning remains visible. Markdown links and
git diff whitespace pass. Ignored evidence lives in build/phase10-verification/:
source-manifest.json, toolchains.json, six build/test logs, stress logs/summaries,
demonstration.log, frozen-compiler-check.log and audit scripts/results.

### Delivery and next work

[LANGUAGE_SERVER.md](LANGUAGE_SERVER.md) documents startup, methods, UTF-16/version
contracts, URI/security limits, compiler dependency boundaries and current gaps.
README, decisions, strategy and known issues are updated. Full graph analysis is
synchronous; only entry-reachable source is semantically validated. Disk reads
remain non-atomic and there is no watcher/save handler/refresh extension. Unsupported
symbol categories return empty results. No broad IDE functionality, new language
features, caches, workers or automatic native publication were introduced.

**Phase 10 is complete locally and ready for hosted GitHub CI.** The existing
workflow already installs Python 3.12, builds all targets and runs all tests with
strict Linux sanitizer settings; no workflow/test skip is needed. The user's next
run must verify the new 21-suite Debug/Release matrix on Linux/Windows/macOS and
Linux sanitizers, especially Windows drive/wide-argument/Unicode filesystem handling
and macOS temporary-root behavior. No new hosted outcome is claimed.

Next consider small editor packaging and explicit disk refresh/cancellation
contracts before workers or partial compilation, retaining exact compatibility and
snapshot ownership. Managed-runtime and package identity gates remain separate.
All version-control and remote publication steps remain with the user.


## Phase 11 — VS Code and VSCodium extension (2026-10-11 local)

Baseline: the user reports Phase 10 commit `ccfea79` passed hosted run #13 on
Linux, Windows and macOS. This phase changes only editor integration, documentation
and additional CI steps. No native C++/CMake source, native test, LLVM version,
language semantics, RTTI correction or reload contract was changed. No commits,
branches, pushes, remote changes or publication were performed.

### Implementation and boundaries

`editors/vscode/` is an isolated strict TypeScript project using pinned
vscode-languageclient 10.1.2, Node.js 24.19.0 and a checked-in-ready npm lockfile.
The native build does not depend on Node. One standard language client owns
stdio, document synchronization, diagnostics and navigation; the existing compiler
owns analysis and UTF-16 ranges. It launches the actual executable directly as
`voidls --entry absolute-entry` with argument arrays and `shell: false`.
The executable transport default is intentional: explicit TransportKind.stdio
would add --stdio, which the existing native CLI does not accept.

Configuration is explicit: void.server.path, void.project.entry and
void.trace.server. Literal relative paths resolve against one workspace folder;
ambiguous Windows drive/root-relative paths are rejected. Entry's parent remains
the canonical compiler root. Empty entry selects a fixed single-file fallback;
switching tabs does not guess a new project, while explicit restart can select
the active file. Multi-root and unrelated documents are explained/rejected or
excluded from synchronization. Native launch is gated on actual Workspace Trust.
Serialized lifecycle work handles restart/configuration changes, preflight errors,
actual spawn failure, unexpected termination and asynchronous shutdown. There is
no crash/restart loop, shell, native download or extension telemetry.

Presentation registers .void, # comments, four-space indentation and a small
TextMate grammar for implemented tokens. It is not another semantic parser.
Completion, hover, rename, formatting, code actions, watchers, workers, new language
syntax and native hot reload remain outside this phase.

### Actual extension verification

Local host: Linux PikaOS, installed VS Code **1.141.0**, Node **24.19.0**, npm
11.6.2. Pinned dependencies installed successfully with ordinary lifecycle scripts
(`npm ci --offline --no-audit`, cached after the initial dependency installation).
Initial dependency audit reported zero vulnerabilities. The final `npm test`
strict type-check, TypeScript/esbuild build and **4/4** configuration/presentation
unit tests passed with zero skipped tests.

The final real editor test command was equivalent to:

```sh
cd editors/vscode
npm ci
npm test
VOIDLS_PATH=/absolute/project/build/ci-debug/voidls \
VSCODE_EXECUTABLE_PATH=/usr/share/code/code npm run test:host
npm run package
```

The installed editor ran on the local graphical display, using disposable profiles
and a real multi-file project under a Unicode root containing spaces, # and %.
UTF-8 comments and an imported `worker space.void` were included. All three final
editor profiles passed:

- Trusted: real native initialization and clean initial diagnostics; definition
  in the imported module and two resolved references; unsaved VOID2002 with exact
  error severity/range and unchanged disk content; no stale definition/reference
  results for invalid source; repair clears diagnostics and restores navigation.
- Trusted lifecycle: tab close/reopen plus actual TextDocument close/open events;
  clean restart and configured entry/executable switches; missing executable;
  actual OS spawn failure when a copied real binary disappears after validation;
  unexpected child termination with no restart loop; fixed single-file fallback
  and explicit restart selection; deactivation and final child cleanup. No
  unhandled client rejection occurred. Normal restart/final shutdown exited zero.
- Restricted: actual workspace.isTrusted was false, explanatory reporting and
  zero native children, including an explicit restart attempt.
- Multi-root: two actual workspace roots, explanatory reporting and zero children.

These are VS Code editor API tests against the built real voidls, not a mocked
LSP server. Test instrumentation observes real child creation and captures error
notification calls; it does not replace native execution or compiler analysis.
The official test runner disables Workspace Trust unconditionally, so the
restricted case launches the same editor through its documented test CLI without
that flag. VS Code may retain text models after tabs close; a documented language
reassignment triggers actual TextDocument close/open events for synchronization.

Earlier attempts exposed the unwanted --stdio argument, retained editor models,
and the official runner's trust flag. Those were investigated and corrected;
initial failing logs remain in ignored evidence. A final npm-ci attempt inside
the execution sandbox was blocked by esbuild's native execution validation
(EPERM); the unchanged pinned installation/build/tests succeeded outside that
sandbox with lifecycle scripts intact. GUI tests also require execution outside
that sandbox. These initial attempts are not counted as passing verification.

### Native regression verification

All existing **21 CTest suites** passed in every configuration, with no skips:

| LLVM 19.1.7 SDK | Build | Passed | Test time |
| --- | --- | --- | --- |
| Installed | Debug | 21/21 | 35.03 s |
| Installed | Release | 21/21 | 30.35 s |
| Installed | ASan/UBSan | 21/21 | 57.20 s |
| Official archive | Debug | 21/21 | 29.33 s |
| Official archive | Release | 21/21 | 25.09 s |
| Official archive | ASan/UBSan | 21/21 | 52.00 s |

Builds used the existing build/ci-debug, ci-release, ci-sanitizers,
ci-official-debug, ci-official-release and ci-official-san configurations.
Debug/Release targets were built before ctest; sanitizer binaries retain the
unchanged native source/configuration. Strict sanitizer execution used
ASAN_OPTIONS=detect_leaks=1:halt_on_error=1:exitcode=87 and
UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1:exitcode=86 outside the tracing
sandbox. The existing SDK-dependent RTTI/vptr correction remains intact.
All native AOT/JIT, GC, reload and Phase 5–10 regressions remain present.

### Package and delivery

`npm run package` succeeded using vsce and the normal prepublish build hook.
The resulting ignored `editors/vscode/dist/void-language-0.1.0.vsix` contains eight
files and is **148706 bytes**. SHA-256:
`313d323ba091fe262e90b3cf1b1cb65e0a86b40b56780a56699714d538375680`.
ZIP inspection verified the packaged JavaScript equals the built bundle and
includes full runtime dependency license notices. No LLVM/native binaries, tests,
node_modules, source maps or lockfile are packaged. The 957.84 KB unminified bundle
produces an informational size warning; the compressed VSIX is 145.22 KB. No
license was invented for the repository: metadata remains UNLICENSED and local
packaging explicitly skips the absent owner license/repository metadata checks.

The actual VS Code CLI installed this VSIX successfully in isolated temporary
extension/profile directories. The user's normal editor installation was not
changed. The editor CLI emitted its own url.parse deprecation warning; installation
succeeded, and the warning remains visible in package-install.log. Build/package/test
output and evidence remain ignored under
build/phase11-verification/, including final dependency/unit/editor/package logs,
native logs, package-audit.json and source-manifest.json.

[EDITOR_INTEGRATION.md](EDITOR_INTEGRATION.md) and the extension README document
native build, explicit settings, real existing source example, development host,
local installation, logging/troubleshooting and limits. Repository README,
DECISIONS, KNOWN_ISSUES, LANGUAGE_SERVER and DEVELOPMENT_STRATEGY are updated.

**Phase 11 is complete locally and ready for hosted CI.** The workflow preserves
the native three-platform matrix and strict Linux sanitizers, then adds pinned
Node dependency installation, type-check/build/unit tests, real editor tests
(Linux xvfb-run), VSIX packaging and per-platform package artifacts. The next
user-triggered run must validate these added steps on all three hosted platforms;
local Linux success does not establish Windows/macOS GUI or hosted download/display
compatibility. VSCodium is not installed locally, and editor 1.91 API compatibility
was type-checked but not exercised in an actual minimum-version host.

Recommended next phase: expanded hosted/editor compatibility checks, followed by
explicit disk-refresh/cancellation and client usability contracts before workers,
partial compilation or broader IDE features. Keep the compiler analysis/reload
contracts and separate managed-runtime/package design gates intact.


## Phase 11 Windows termination-report correction (2026-10-11 local)

### Hosted evidence and root cause

Read the actual GitHub Actions logs for [run #14, Windows job 114668718372](https://github.com/Shmellyorc/Void.Language/actions/runs/38205742510/job/114668718372),
commit `7fa9893d3aadc2cab5d1588f8cafefa21754860f`, using the read-only GitHub
job-log API. Windows Server 2022 native Debug and Release passed 21/21 each;
extension unit tests passed 4/4. The editor test reached stopped after killing
the actual native process, then failed with `Timed out: termination report` at
04:59:04 UTC. The logs did not capture the client output channel/underlying Windows
pipe error. Consequently an exact Windows errno or event timeline cannot be
asserted from this log alone. Focused stream/exit/notification instrumentation now
prints these events in the integration test for the next hosted run.

The pinned vscode-languageclient 10.1.2 implementation establishes the lifecycle
hole: common/client.js handleConnectionError awaits error(), then ErrorAction.Shutdown
calls stop(). shutdown() enters Stopping before asynchronously awaiting shutdown/exit.
handleConnectionClosed skips errorHandler.closed() while Stopping and returns early
when Stopped. VOID's old error() returned Shutdown without reporting, while only
closed() reported unexpected termination. A transport error before close therefore
stopped the client without the promised VOID notification. Windows terminated pipes
can exercise this ordering, unlike the close-first path observed locally. This
ordering explains the hosted symptom; its precise Windows OS event remains to be
confirmed by the added instrumentation on the next run.

Before editing extension lifecycle code, reproduced this exact reporting hole in
an actual Linux VS Code host with the real built voidls: emit a controlled EPIPE
error on the actual child stdin transport, allowing the standard language client's
real error handler to stop the native server. The unmodified extension reached
stopped and failed `Timed out: transport error termination report`. This is a
controlled transport-event regression, not a claimed Windows OS reproduction,
fake server, replacement transport or compiler simulation. The original real
process-kill assertion remains and is strengthened to count reports per failure.

### Exact correction and regression coverage

The final error() policy returns Continue with handled=true and logs the transport
error; an error alone is not proof of native termination. It leaves the standard
client eligible to invoke closed() when the actual connection closes. closed()
reports through a per-client guarded reporter and returns DoNotRestart/handled=true,
so the standard client cleans up without restarting or a duplicate generic popup.
A successfully initialized current session reports once; later independently failed
sessions still notify. Startup failures retain their existing startup-error path.
Intentional shutdown is identified by manager disposal or clearing/replacing the
active client before stop(), suppressing false termination warnings for explicit
restart/configuration/deactivation. The normal asynchronous stop/dispose lifecycle
remains unchanged. No separate process supervisor/transport is introduced.

A first correction reported from error() as well as closed() while retaining
Shutdown. Stronger assertions exposed a generic duplicate popup during an actual
local process kill: handleConnectionClosed uses a default unhandled DoNotRestart
result when Stopping, bypassing our closed() result. Thus reporting from both
callbacks was insufficient. The final Continue policy avoids entering Stopping
from an error and preserves the handled close path. The intermediate attempt is
not counted as passing final verification; its output-channel evidence is retained
in windows-fix-duplicate-warning.log.

The real editor suite additionally injects two controlled stream errors on the
actual native child's stdin, verifies that a transport error alone neither stops
the client nor falsely reports termination, then kills that actual process to
force error-before-close ordering. It requires exactly one notification, real
child cleanup and no automatic restart. After explicit restart, the original
independent native process-kill test remains and must produce the second report.
Exactly one UI notification per failure is asserted, including generic client
popups; normal initialization/restart/entry changes must not warn. Subsequent
single-file selection may legitimately warn about other open project documents,
but no failure notification is permitted during intentional restart/deactivation.
Existing diagnostics, unsaved repair, navigation, startup-failure, trust, multi-root,
cleanup and unhandled rejection assertions remain. Instrumentation logs real stream
errors, native exit status and captured error notifications to stdout/evidence;
the VOID channel logs connection reasons and lifecycle state.

### Local results and remaining hosted check

Using Node 24.19.0, VS Code 1.141.0 and the installed LLVM 19.1.7-built real voidls:

- `npm test`: strict type-check/build and 4/4 unit tests passed.
- `VOIDLS_PATH=.../build/ci-debug/voidls VSCODE_EXECUTABLE_PATH=/usr/share/code/code npm run test:host`:
  trusted (controlled error-before-close and independent process kill), restricted and multi-root profiles passed.
  Exactly one report per unexpected failure, no termination warning for intentional
  shutdown, no automatic restart, no unhandled rejection and no leaked child.
- `ctest --test-dir build/ci-debug --output-on-failure`: 21/21, 34.85 seconds.
- `ctest --test-dir build/ci-release --output-on-failure`: 21/21, 30.45 seconds.
- Strict installed-SDK ASan/UBSan CTest: 21/21, 54.21 seconds, with leak detection
  and halt-on-error settings unchanged, outside the tracing sandbox.
- `npm run package`: succeeded; eight-file local VSIX includes dependency notices
  and the exact built bundle. Package/source hashes are recorded in
  build/phase11-verification/windows-fix-audit.json. The informational bundle-size
  warning remains visible.

Ignored evidence is in build/phase11-verification/windows-job-excerpt.log and
windows-fix-{before,after,unit,package,native-debug,native-release,native-sanitizers}.log.
The before log is an intentional failing reproduction, not a passing result.
No compiler/native-server/LLVM/configuration/workflow/dependency changes were made.
All existing tests remain intact. Windows is unavailable locally: the actual Windows
process termination/pipe behavior still requires another user-triggered hosted run;
macOS was not rerun locally either. No hosted success is claimed for this correction.
No commits, branches, pushes, remote edits or publication were performed.


## Phase 11 run #14 Linux evidence review (2026-10-11 local)

Reviewed the actual read-only GitHub job logs and completed run metadata for
[run #14](https://github.com/Shmellyorc/Void.Language/actions/runs/38205742510),
commit `7fa9893d3aadc2cab5d1588f8cafefa21754860f`:

- [Linux job 114668718282](https://github.com/Shmellyorc/Void.Language/actions/runs/38205742510/job/114668718282):
  Debug, Release and strict ASan/UBSan each passed 21/21; extension unit tests
  passed 4/4. The real editor test failed `Timed out: termination report`.
- [Windows job 114668718372](https://github.com/Shmellyorc/Void.Language/actions/runs/38205742510/job/114668718372):
  rechecked Debug/Release 21/21 and unit 4/4 passes, with the same editor timeout.
- macOS job 114668718102 completed successfully, including real editor integration
  and VSIX packaging, as confirmed by the run job/step metadata.

Both failing jobs exercised the original committed lifecycle, not the existing
uncommitted correction. Linux restored and initialized the real server at
05:34:48.393–.398 UTC, logged `Error: Unexpected SIGPIPE` at 05:34:48.402,
then remained alive until the termination-report assertion timed out at
05:35:08.454. Its stack is process.<anonymous> in the editor's
resources/app/out/bootstrap-fork.js:4:1710 followed by process.emit, not a native
VOID/LLVM backtrace. The original test awaited the client reaching stopped before
waiting for the notification. This matches the same error-before-close reporting
hole investigated above; neither log captures the exact transport errno/callback
sequence, so that fine-grained Windows/Linux event order remains an inference
supported by the pinned client source and deterministic local reproduction.

Inspected the installed matching VS Code 1.141.0 bootstrap-fork.js. Its SIGPIPE
handler uses process.on('SIGPIPE', ...) and console.error(new Error('Unexpected
SIGPIPE')), once per process. It does not throw that Error or terminate the host.
A write to a killed child's closed pipe is consistent with the test timing; the
log does not identify the individual write. The host continuing for twenty seconds
and failing the notification assertion confirms this message was not itself the
fatal assertion or an extension-host crash. No native sanitizer issue is indicated.

Crucially, the final corrected local real-editor run already recorded the same
`Unexpected SIGPIPE` once in windows-fix-after.log and still passed the trusted,
restricted and multi-root profiles. The real kill, exact notification counts,
no-automatic-restart, intentional-shutdown and child-cleanup assertions all passed
with that signal log present. No signal handler, environment flag, suppression,
weakened assertion or test skip was added. Hashes of extension.ts and host.ts still
match the verified windows-fix-audit.json. The earlier local 4/4 unit, three 21/21
native configurations and package results remain applicable because code is
unchanged. No redundant native or editor rerun was needed for this evidence-only
review. The Linux CI excerpt is retained in ignored
build/phase11-verification/linux-run14-excerpt.log.

**The existing correction covers the lifecycle hole behind both failures and is
ready for the next hosted CI attempt.** ErrorAction.Continue logs a transport error
without prematurely entering Stopping, allowing actual connection closure to run
the guarded reporting handler and DoNotRestart cleanup. macOS close-first behavior
continues to use that same handler. SIGPIPE may still be logged during deliberate
process-kill testing; current evidence does not justify a separate product change.
A new hosted run is still required to verify the corrected code on the actual
Linux/Xvfb, Windows and macOS runners. Exact Windows pipe behavior remains unavailable
locally. This review changes WORK_LOG.md only; the existing lifecycle correction,
all strict tests, native code, LLVM settings and workflow are left intact. No
commits, pushes, branches or remote modifications were performed.


## Phase 11 run #15 Windows entry-path correction (2026-10-11 local)

### Actual hosted evidence

Read [run #15](https://github.com/Shmellyorc/Void.Language/actions/runs/38209988399)
and [Windows job 114681780204](https://github.com/Shmellyorc/Void.Language/actions/runs/38209988399/job/114681780204)
through the read-only GitHub API. Commit is
`6142383e584d5803ea9a20f1de60b3b69204ef5b`. Native Debug/Release each passed 21/21
and extension unit tests passed 4/4. Both real native process-kill scenarios
produced the expected termination notifications (05:58:04.007 and .240 UTC),
confirming progress past the previous termination timeout on Windows.

The new failure at out/test/host.js:256 is exact string inequality in the fallback
entry assertion: actual `C:\Users\runneradmin\...\worker space.void`, expected
`c:\Users\runneradmin\...\worker space.void`. Every other character, including
Unicode root, spaces, escaped punctuation and filename, matches. The native child
was launched successfully with the canonical worker entry and published worker
file diagnostics immediately before this assertion. Evidence is retained in
ignored build/phase11-verification/run15-windows-excerpt.log.

### Contract and minimal correction

This assertion compared two representations of the same entry. The existing
resolveProject contract uses the filesystem-canonical parent (fs.realpath) joined
with the selected basename, retaining a native filesystem path for --entry and
status. VS Code Uri.fsPath lowercases a Windows drive letter. The test fixture
already supplies its filesystem-canonical root independently via VOID_TEST_ROOT;
assertions now compare status against exact path.join(root, 'worker space.void')
and path.join(root, 'main.void'). They still require the precise expected root and
filename, fixed fallback after switching tabs, and new entry after explicit restart.
No generic case-insensitive comparator or whole-path lowercasing was introduced.
No native entry selection, project root, resolver or compiler identity is changed.

Inspection also found a real editor-facing warning defect adjacent to the assertion:
the single-file exclusion check compared doc.uri.fsPath directly against native
project.entry. On Windows it could falsely warn that the selected file itself was
outside its session because of C:/c: spelling. That one comparison now converts
the native entry through vscode.Uri.file(entry).fsPath before comparing editor
paths. This uses the established editor representation without folding directory
or basename case, changing filesystem identity rules or widening project scope.
The native entry/status representation remains unchanged.

Added real-editor API checks, portable across hosts, proving upper/lower Windows
drive URI spellings yield the same fsPath while different drives, directory case
and filenames remain distinct. The real fallback scenario temporarily closes the
main VOID document via language reassignment, leaving only the selected worker
VOID document. Selecting fallback must add no outside-project warning. Reopening
main as VOID restores the existing tab-switch/restart checks. These additions keep
actual source, Unicode/space paths, native children, strict notification counts,
clean deactivation and every previous compiler/lifecycle assertion intact. They
are actual URI API tests, not a claim of running Windows filesystem APIs on Linux.

### Local verification and pending hosted results

- `npm test`: strict type-check/build and all 4 unit tests passed.
- Real installed VS Code 1.141.0 against the LLVM 19.1.7-built native voidls:
  trusted, restricted and multi-root profiles passed, including URI checks,
  selected-file warning regression, both process kills, exact notifications,
  fallback selection, explicit restart and final child cleanup.
- Debug CTest rerun: 21/21 passed, 34.40 seconds. Native C++/tests, sanitizer
  configuration and LLVM/Clang/LLD 19.1.7 were not modified. The preceding local
  Release/sanitizer validation remains applicable to those unchanged sources.
- `npm run package`: succeeded, eight-file VSIX, 145.41 KB. ZIP audit confirms
  the shipped JavaScript equals the tested build; source/package hashes are in
  build/phase11-verification/run15-audit.json. Informational bundle-size warning
  remains visible. git diff whitespace check passed.

Local evidence is in build/phase11-verification/run15-{unit,editor,native-debug,package}.log.
At the latest run #15 job review, macOS job 114681780024 was still building Release;
Linux job 114681780334 was configuring Release. Neither completed result was yet
available, so neither platform is claimed green for run #15. Windows is unavailable
locally; the drive URI behavior is exercised through actual VS Code APIs, while the
full native Windows fallback/warning sequence requires the next hosted run. The
correction is ready for that attempt. No workflow/dependency/compiler/runtime
changes, commits, branches, pushes or remote modifications were performed.
# Phase 12 review — 12 October 2026

The project owner reports Phase 11 Actions run #16 at `1b32151` passed Linux,
Windows and macOS. Phase 12 begins from that completed foundation with a clean
working tree. Reviewed the language/value-semantics contracts and traced the
existing frontend, immutable validated analysis, module resolution, scalar VOID
IR, shared LLVM lowering, tooling and strict SDK/sanitizer configuration.

Value structs require extending the shared type representation, resolving named
types, aggregate IR operations and aggregate debug metadata; primitive-only types
currently flow through all these layers. No parallel type checker/backend is
needed. The existing i32 host adapter can stay intact while source functions
exchange aggregates internally.

Recorded the bounded proposal in VALUE_STRUCTS_PROPOSAL.md. Approval is pending
for pure compile-time declared field defaults versus runtime defaults with an
explicit observable construction sequence. The Bible establishes safe defaults
and named construction, but does not settle runtime default expressions; §16 also
leaves custom initializer interaction open. This is a design gate requested by
the owner, not an implementation failure. No compiler/runtime/editor changes,
Phase 12 builds or tests, commits, branches or remote modifications have been
made during this review.

## Phase 12 implementation and final local validation — 12 October 2026

The owner approved Option A in VALUE_STRUCTS_PROPOSAL.md after the design review.
Implemented pure, typed compile-time declared field defaults, recursive safe
fallbacks and source-order runtime named arguments. Runtime defaults, custom init
and positional constructors remain deferred; none is permanently prohibited.
Phase 11 run #16 at `1b32151` passed all hosted platforms according to the owner.
Phase 12 hosted results are not yet available.

### Implemented surface and architecture

- Numeric/bool and acyclic nested value structs; forward and imported type names;
  public/internal/file-private type declarations; exact nominal module identity.
- Optional named-field construction, compile-time aggregate constants, recursive
  defaults, field reads, nested writes/compounds/increment/decrement, local copies,
  mutable copied parameters and independent aggregate returns. Readonly binding
  roots protect all nested fields. Named arguments evaluate once in written order.
- One shared nominal descriptor model extends ir::Type through frontend, validated
  analysis, independent IR and LLVM. ConstantValue carries recursive values, while
  scalar evaluation retains the original numeric kernel. Layout/default dependency
  gates reject cycles and unsupported types before lowering. Limits bound layout
  depth/default dependency recursion and expanded layout size; expression limits
  reset for each declared field default.
- Aggregate construct/extract/insert IR operations and typed slots implement copy
  semantics with no heap/reference aliasing mechanism. Shared LLVM lowering uses
  target-aligned declaration-order structs and matching private source signatures.
  Target DataLayout is available before aggregate debug member sizes/offsets are
  built. Ordinary runtime failures propagate through aggregate-returning functions
  before callers consume their result. The host adapter remains i32(i32).
- Validated analysis owns type/field declarations and references. Qualified type
  annotation navigation points at the type identifier; annotation dependency edges
  identify their enclosing declaration, preserving tooling invalidation contracts.
  Constructor names/field initializers and field reads/writes navigate through the
  existing LSP index. Layout/type/default changes reject reload before publication;
  compatible function-body changes still reload and preserve old leases.
- Editor grammar/indentation recognizes contextual `value struct`; ordinary `value`
  parameters remain valid. Extension lifecycle code, compiler/runtime boundaries,
  LLVM version/SDK requirements, RTTI correction and native printf proof are intact.

During implementation, existing regressions exposed contextual-keyword and import
lookup/recovery mistakes. These were corrected in the compiler rather than by
changing their assertions. Final runs preserve malformed signature recovery,
failed-import poisoning, ambiguity locations, independent diagnostics, and numeric
context from qualified constants. Added struct tests also check parser recovery,
invalid defaults/fields/types/returns, recursion/resource limits and malformed IR.

### Exact final commands and results

Existing build trees retain their strict LLVM 19.1.7 configurations: installed SDK
at `/usr/lib/llvm-19/lib/cmake/llvm`, matching `/usr/bin/clang++-19`; official SDK
at `.toolchains/ci-investigation/LLVM-19.1.7-Linux-X64/lib/cmake/llvm`, with its own
matching Clang/LLD. CMake reconfiguration during this phase passed the required
ORC link-and-run probe. SDK headers, libLLVMOrcJIT.a and LLVMConfig.cmake were
checked for both installations; llvm-config reports 19.1.7 for both, RTTI YES
for installed and NO for official. No mismatched components or relaxed SDK
requirements were introduced. See build/phase12-verification/sdk.json.

For each Debug/Release tree below, executed:

```sh
cmake --build build/<tree> -j 2
ctest --test-dir build/<tree> --output-on-failure -j 4
```

For each sanitizer tree, built with the same command, then executed outside the
tracing sandbox so LeakSanitizer can operate:

```sh
env ASAN_OPTIONS=detect_leaks=1:halt_on_error=1:exitcode=87 \
    UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1:exitcode=86 \
    ctest --test-dir build/<tree> --output-on-failure -j 4
```

| SDK | Build tree / configuration | Final result | CTest elapsed |
| --- | --- | --- | --- |
| Installed, RTTI on | ci-debug / Debug | 22/22 passed | 26.91 s |
| Installed, RTTI on | ci-release / Release | 22/22 passed | 19.83 s |
| Installed, RTTI on | ci-sanitizers / RelWithDebInfo ASan/UBSan | 22/22 passed | 30.98 s |
| Official, RTTI off | ci-official-debug / Debug | 22/22 passed | 22.71 s |
| Official, RTTI off | ci-official-release / Release | 22/22 passed | 15.97 s |
| Official, RTTI off | ci-official-san / RelWithDebInfo ASan/UBSan | 22/22 passed | 26.81 s |

These are 132 passing suite executions. All original 21 suites remain; the new
value_structs_cli suite adds normal/O2 JIT/AOT execution, actual native linking and
DWARF verification for three source programs. The frontend suite adds struct
semantics, diagnostics, nominal module lookup, readonly, tooling, compatibility,
resource and IR validation cases. Existing numeric/GC/reload/native and Phase 5
recovery assertions remain enabled. Sanitizer instrumentation retains the existing
SDK-compatible RTTI policy; C++ compiler/runtime checks do not imply that generated
LLVM/JIT application code is sanitizer-instrumented (the existing limitation).

Extension verification, with Node 24.19.0 and existing pinned dependencies:

```sh
cd editors/vscode
npm test
node --test --test-reporter=tap out/test/config.test.js
VOIDLS_PATH=/home/shmellyorc/Documents/Void.Language/build/ci-debug/voidls \
VSCODE_EXECUTABLE_PATH=/usr/share/code/code npm run test:host
```

Strict TypeScript checking/build passed. All four registered unit tests passed
(the detailed TAP rerun outside the sandbox confirms their individual execution).
Real installed VS Code 1.141.0 passed trusted, restricted-trust and multi-root
profiles against the final native voidls, including actual process kills, strict
notification counts, no automatic restart, fallback paths, explicit restart and
clean child shutdown. The existing intentional-kill SIGPIPE message remains visible
in the passing trusted-host log; no lifecycle change or suppression was added.
The native LSP protocol suite additionally passed type/constructor/field navigation,
references, readonly diagnostics and invalid-to-valid revision recovery.

### Actual source-to-native demonstration

Executed the documented check/run/emit commands on
examples/value_structs/main.void, linked its emitted object with the matching
Clang/LLD 19.1.7, then ran the resulting Linux ELF executable:

```text
Source checked successfully
46                          # normal JIT, input 10
46                          # optimized JIT, input 10
Wrote native source object: build/phase12-verification/value-structs.o
46                          # separately linked native executable, input 10
```

The object/executable and native-demo.log are retained under
build/phase12-verification/. `file` identifies the executable as an x86-64 ELF PIE
with debug_info. This compiles the actual imported .void declarations, fields,
functions and expressions; no manually reconstructed example IR is used.

Final per-tree build/test logs, extension/unit/editor logs, SDK evidence, source
hashes and results.json are retained in build/phase12-verification/. git diff
whitespace validation passed. Build artifacts are local ignored outputs.

### Remaining limitations and hosted readiness

VALUE_STRUCTS.md, SOURCE_COMPILER.md, VALUE_SEMANTICS.md, DECISIONS.md,
KNOWN_ISSUES.md, DEVELOPMENT_STRATEGY.md and README now describe the implemented
surface and limits. The Language Bible was not altered. No public C aggregate ABI,
stable offsets/serialization, managed reference types, runtime field defaults,
custom init, positional constructors, field modifiers or aggregate equality was
introduced. The GC remains experimental. The recommended next phase is receiver
methods and initialization policy, including readonly receiver semantics and the
observable construction sequence, before custom init or managed fields.

Phase 12 is ready for the next hosted CI attempt. All verification above ran on
Linux x86-64; Windows and macOS aggregate calling/layout behavior and their real
editor scenarios still require hosted execution. The unchanged workflow runs the
new 22nd suite automatically in its Debug/Release and Linux sanitizer matrices.
No commits, pushes, branches, publishing or remote modifications were performed.


## Phase 13 — receiver methods and initialization groundwork (2026-10-12)

### Approved scope and implementation

The owner approved **M1 + I1** from RECEIVER_METHODS_PROPOSAL.md. The owner reports
Phase 12 hosted run #18 passed Windows, Linux and macOS at baseline `acf7355`.
This phase implements methods on supported nominal value structs in their owning
source module/file, including calls through direct/aliased imports and project-root
types. Foreign-type extension declarations remain deferred. The implemented
contract is documented in RECEIVER_METHODS.md and the approved additions to Bible
sections 14.2 and 16; M2/I2 alternatives were not silently adopted.

Working syntax is `func (p point) move(dx: i32):` and `p.move(1)`. Ordinary
parameters keep their existing `name: type` syntax and exact contextual argument
checking. Methods support all existing numeric/Boolean/nested value types,
ordinary statements, recursion, aggregate arguments and aggregate returns.

The parser retains receiver expressions until semantic resolution distinguishes
module-qualified calls from value access. Methods share function registration,
argument checking, nominal types, validated bodies and the existing small
expression handlers. Owner/type identity determines lookup; duplicate methods,
field collisions, invalid owners, visibility, receiver/argument/return mistakes
produce structured source-located diagnostics. Unknown methods still analyze
arguments for independent errors. New codes VOID2018–2020 describe invalid
receiver storage, unknown methods and deferred initialization respectively.

Validated receiver-rooted writes seed a finite reverse-call-graph effect analysis.
Indirect nested calls and mutually recursive methods propagate mutability;
independent local and ordinary parameter copies do not alias. Readonly receivers
accept non-mutating calls and reject direct/indirect mutating calls. Non-mutating
calls can materialize temporaries once; mutating calls require actual mutable
local/parameter-rooted storage. Receiver evaluation precedes arguments, which
execute exactly once left to right. Ordinary aggregate arguments remain snapshots.

The one backend-independent IR adds a typed receiver Place (slot plus checked
nested field indexes) and method receiver/effect metadata. The validator checks
signatures, ownership paths and non-mutating receiver writes/calls. Shared LLVM
lowering aliases receiver slot zero to an internal storage pointer; remaining
parameters retain value copies. This avoids delayed copy-back overwriting
argument-side mutations, including replacement of a containing aggregate. JIT
and AOT use the same code and DataLayout, including mixed padded nested values.
No external aggregate C ABI, escaping receiver reference, alias promise, new runtime
system or GC claim was introduced. Existing failure-context checks include method
source locations and caller traces.

Owned analysis/tooling declarations expose receiver kind/type, nominal owner and
inferred effect. Navigation/references, unsaved revisions and LSP diagnostics use
that shared representation. Exact hot-reload compatibility includes method identity,
receiver type/effect and existing signature/visibility/layout/default contracts.
Compatible body edits reload while old leases retain their code/source. Effect
changes require restart and cannot publish. Extension production/lifecycle code
was unchanged; its indentation test now also covers receiver declaration syntax.

### Initialization decisions explicitly deferred

Phase 12 safe defaults, pure declared defaults and runtime named-field construction
remain unchanged. Receiver `init` declarations and `.init()` calls diagnose the
unimplemented lifecycle feature rather than execute as ordinary methods. Custom
`deinit` is also outside this slice. This does not permanently prohibit lifecycle
methods. Compiler checks reject invalid initialization before object output and
preserve an existing output file.

RECEIVER_METHODS.md records the remaining approval gate: named overrides versus
initializer order, initializer parameters versus fields, nested construction/default
rules, typed uninitialized bindings, pure constant/default contexts, lifecycle
visibility, exactly-once publication and initializer compatibility. I2 is an
unapproved proposal only. The recommended next phase is a coherent construction
contract before custom initializer execution; runtime field defaults, managed
reference types and allocation remain future work.

### Final local native validation

All results below are from the final production/test sources, including shared
function/method argument validation and independent unknown-argument recovery.
All **22 original suites remain enabled**, plus receiver_methods_cli, for **23**.
Each complete build succeeded before its full test matrix:

| SDK | Build configuration | Full result | Elapsed test time |
| --- | --- | --- | --- |
| Installed LLVM 19.1.7 | Debug | 23/23 | 18.03 s |
| Installed LLVM 19.1.7 | Release | 23/23 | 34.25 s |
| Installed LLVM 19.1.7 | RelWithDebInfo, ASan/UBSan | 23/23 | 46.50 s |
| Complete official LLVM 19.1.7 archive | Debug | 23/23 | 32.06 s |
| Complete official LLVM 19.1.7 archive | Release | 23/23 | 28.96 s |
| Complete official LLVM 19.1.7 archive | RelWithDebInfo, ASan/UBSan | 23/23 | 38.59 s |

Thus the six complete matrices passed **138 suite executions**. Both SDKs have
verified LLJIT headers, LLVMOrcJIT library and LLVMConfig.cmake, and report version
19.1.7. The installed SDK reports RTTI YES; the complete official archive reports
RTTI NO. The existing consistent SDK-wide RTTI correction was preserved. Fresh
configuration in both Debug trees passed the real `VOID_LLVM_ORC_RUNS` compile/link/
execution probe. Official builds use their matching archive Clang/LLD; installed
builds use installed Clang/LLD 19.1.7. No dependency requirement was weakened.

Build/test commands (the six configured trees retain reproducible CMake caches):

```sh
cmake --build build/ci-debug -j 4
ctest --test-dir build/ci-debug --output-on-failure -j 4
# Repeat build/test for ci-release, ci-official-debug and ci-official-release.
cmake --build build/ci-sanitizers -j 4
ASAN_OPTIONS=detect_leaks=1:halt_on_error=1:exitcode=87 \
UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1:exitcode=86 \
ctest --test-dir build/ci-sanitizers --output-on-failure -j 4
# Repeat the strict sanitizer build/test for ci-official-san.
```

ASan, UBSan and leak checks stayed strict, without new suppressions or exclusions.
Sanitizer processes ran outside the filesystem sandbox so LeakSanitizer can inspect
processes. The installed RTTI-enabled SDK retains vptr instrumentation; the official
no-RTTI SDK retains the established matching configuration, without pretending
vptr checks exist there. Compiler/runtime instrumentation does not imply generated
application machine code is sanitizer-instrumented. Initial LLVM header compilation
emitted its existing `ciso646` deprecation warning with host standard-library headers;
no warning suppression was added.

Both Debug frontend tests ran under the unchanged **1 MiB stack** requirement,
then each passed **five consecutive repetitions** (ten additional suite executions).
This includes the existing Phase 12 recursive layout/resource guards, plus a
180-method effect SCC and a 400-link receiver expression rejected at the nesting
limit with independent later recovery. No resource or stack limits were raised.
Both sanitizer SDKs additionally passed frontend and reload **three consecutive
repetitions each** (twelve additional suite executions). Commands:

```sh
# Run in each of ci-debug and ci-official-debug:
(ulimit -s 1024; ctest --test-dir build/ci-debug -R '^frontend$' --repeat until-fail:5 --output-on-failure)
# Run with the strict sanitizer environment above in both sanitizer trees:
ctest --test-dir build/ci-sanitizers -R '^frontend$|^reload$' --repeat until-fail:3 --output-on-failure
```

The frontend method group checks actual storage, whole/nested replacement, readonly
locals/nested fields/module constants, temporary snapshots, contextual arguments,
parameter/copy independence, evaluation order, owner imports, private/transitive
visibility, same-spelling nominal methods, recursion/effects, parser recovery,
source diagnostics, method navigation, compatible reload/old leases, effect-change
rejection and malformed receiver IR. Existing numeric/diagnostic/GC/native/reload
assertions remain intact.

The new real-source CLI suite reuses the existing native value-source driver.
Five independent projects cover imported nested point/box values, side effects and
snapshots, recursive methods, all ten numeric types plus Boolean/padded aggregates,
and runtime failure traces. It exercises normal/O2 JIT and actual linked AOT,
multiple inputs, emitted-object DWARF validation and rejected custom initialization.
The native LSP suite additionally verifies imported receiver/type definitions,
references, exact readonly ranges, unsaved effect changes and invalid/valid recovery.

### Editor and packaging validation

With Node 24.19.0 and existing pinned dependencies, strict TypeScript checking/build
and **all four extension unit tests passed**. The detailed TAP rerun confirms
individual execution. Local VSIX packaging succeeded:
`editors/vscode/dist/void-language-0.1.0.vsix` (8 files, 145.43 KB).

```sh
cd editors/vscode
npm test
node --test --test-reporter=tap out/test/config.test.js
VOIDLS_PATH=/home/shmellyorc/Documents/Void.Language/build/ci-debug/voidls \
VSCODE_EXECUTABLE_PATH=/usr/share/code/code npm run test:host
npm run package
```

Real installed VS Code **1.141.0** passed trusted, restricted-trust and multi-root
profiles using the final native voidls. Actual child-process kills, strict
notification counts, no automatic restart, explicit restart, Unicode/fallback
paths and clean deactivation remain exercised. No fake server or relaxed lifecycle
assertion was substituted.

### Actual source-to-native demonstration and hosted readiness

Executed check, normal JIT, optimized JIT, object emission and matching Clang/LLD
linking against the final compiler, on `examples/receiver_methods/main.void`:

```text
Source checked successfully
301                          # normal JIT, input 10
301                          # optimized JIT, input 10
Wrote native source object: build/phase13-verification/receiver-methods.o
301                          # separately linked native executable, input 10
```

`file` identifies the result as an x86-64 ELF PIE with debug_info. This compiles
real imported .void types, methods, mutation, copies, functions and expressions;
no special-case example IR was reconstructed. Commands are in RECEIVER_METHODS.md.
Final per-tree logs, SDK checks, stress/editor/package evidence, native outputs and
source hashes/results.json are retained locally under build/phase13-verification/.
Git whitespace validation passed; generated evidence/artifacts are ignored outputs.

Phase 13 is **ready for hosted CI**, with approval limited to M1 + I1. All execution
above was Linux x86-64. Windows and macOS receiver pointer/layout/calling conventions,
Debug/Release matrices and real editor scenarios still require hosted execution;
Linux hosted sanitizers must also confirm the official archive environment. The
existing workflow discovers all 23 suites automatically. No commits, pushes,
branches, publishing or remote modifications were performed.

## Phase 14 construction contract proposal — awaiting approval (2026-10-12)

The owner reports Phase 13 `c72e62a` passed hosted run #19 on Windows, Linux and
macOS. The current local HEAD is its subsequent syntax-highlighting correction
`ffa9cdf`; the working tree was clean. Reviewed the current design/technical
documents and the actual parser, parameter/call checker, structural-default
evaluation, typed-local construction, receiver lowering and analysis contracts.

Created INITIALIZATION_PROPOSAL.md, explicitly unapproved. Recommended C1 binds
custom constructor arguments only to initializer parameters, supports typed
positional/named arguments and pure defaults, evaluates supplied expressions
before recursive field construction, and completes child initializers before the
parent body. It describes missing-required-child/default availability, typed local
construction, pure-context rejection, visibility/private factories, once-only
publication/failure propagation and conservative initialization/helper reload
dependencies. Alternatives and limitations are stated for owner selection.

Identified a direct conflict with preserving every historical assertion: one
frontend test and the CLI check/run/emit case expressly reject a valid init
declaration because Phase 13 deferred the feature. The proposal requests explicit
approval for those narrowly scoped expectation updates while retaining direct
lifecycle-call rejection, strict failures and existing-output preservation.

Stopped before compiler behavior changes as explicitly requested for consequential
public construction rules. Only the proposal and this log entry changed. Git
whitespace validation passed; no build/test rerun is claimed for this documentation
gate, and the six Phase 13 matrices are not Phase 14 evidence. Implementation,
INITIALIZATION.md and full Phase 14 validation follow approval. No commits, pushes,
branches or remote modifications were performed.


## Phase 14 — approved custom value initialization and editor correction (2026-10-12)

### Approval, baseline and implemented contract

The owner approved **C1** in INITIALIZATION_PROPOSAL.md, explicitly including
recursive nested construction, required-child default availability, constructor
visibility, conservative initialization/helper reload dependencies and the specific
obsolete Phase 13 rejection-test updates. No I2 alternative was adopted. The owner
reports baseline Phase 13 `c72e62a` passed hosted run #19 on all platforms; this
checkout also includes the owner's subsequent `ffa9cdf` syntax-highlighting change.

INITIALIZATION.md records the working contract. Owner-module custom initializers
accept explicit typed positional/named parameters and pure defaults, with required
parameters before optional ones. Constructor arguments exclusively bind parameters;
no-init types retain named fields. Duplicate/unknown/double binding, order, count,
type, lifecycle result/declaration and access mistakes diagnose before lowering.
Supplied expressions snapshot once in source order before any nested initialization.
Fresh fields construct in declaration order, then init runs once on prepared storage.
Readonly applies on successful completion. Copies/ordinary arguments/returns never
repeat init, and failures do not publish the fresh result or overwrite its destination.

Omitted nested values recursively run available default construction; explicitly
supplied children do not first execute a discarded default initializer. Required
children can be supplied to no-init wrappers. Custom parents must have safe defaults
for all fields before their bodies; invalid declarations identify the field and
required child parameter. Merely declaring/using a required-argument type does not
poison its layout. Typed locals use the same zero-argument construction/access rules.
Pure module/field/parameter defaults cannot execute init, directly or implicitly;
local runtime const construction is valid. Private factories and owner-encapsulated
nested construction work without granting callers private initializer access.

The parser/Function/Parameter/shared nominal Type model owns these rules. One
argument checker performs contextual typing for functions/methods/initializers;
bounded construction plans distinguish pure structural storage from runtime default
availability. Typed slot snapshots, aggregate operations and Phase 13 receiver Places
lower through the unchanged shared LLVM backend. IR initializer signatures and calls
carry explicit invocation metadata and reject ordinary invocation or inappropriate
receiver paths. Existing failure checks guard use of constructed results. No second
frontend, type system, constructor code generator, heap or managed runtime was added.

Shared owned analysis adds initializer kind, parameter/default source metadata,
constructor references and implicit construction dependencies. Implicit edges do
not obscure written navigation/references. Exact compatibility freezes init
presence/signature/labels/defaults/access/effects/body and transitive helper bodies,
using an iterative closure including cycles. Unrelated compatible bodies still
reload, retaining old leases; changes to construction policy require explicit restart.
Existing default/layout/constant/binding rules and source resource guards remain.

### Windows run #20: actual log and isolated correction

Read the completed Windows job **114796125401** through the GitHub API for run
**38244868927**. The whole run was initially still in progress, so run-level logs
were unavailable; the completed job log was retrieved separately and is retained
as ci20-windows.log. At 2026-10-12T11:50:38Z, the presentation unit test failed at
compiled out/test/config.test.js:98 with `keyword.test(word)` false. Three of four
unit tests passed. The first keyword.control.void rule is now the intentionally
approved optional-value/struct rule; `.find()` therefore never examined func/return.
The original negative struct assertion also contradicted intentional highlighting.

Only editors/vscode/src/test/config.test.ts changed for this correction. It filters
all applicable keyword rules, checks func/return and prior working keywords, and
requires exact complete matches for both struct and value struct. Unsupported
while/async/match/defer and misleading prefixes/standalone value remain excluded.
No grammar rules/colors, indentation rules, extension lifecycle or compiler semantics
were changed to resolve the editor regression. The actual native process-kill tests,
notification counts and shutdown/restart behavior remain intact.

### Regression coverage and actual native source programs

All **23 existing suites remain enabled**, plus initialization_cli: **24 total**.
The specifically approved valid-init rejection in frontend/methods.cpp is now
positive source execution. The old CLI deferral fixture now defines a valid init
and attempts an invalid direct p.init() call. Its check/run/emit status/VOID2020 and
protected-output assertions remain, as does direct-call rejection in frontend tests.
No other historical assertion was removed or relaxed.

Seven actual projects under examples/initialization exercise:

- main: imported nested defaults/initializers, named arguments, typed locals,
  readonly/copies, methods, owner-private nested construction and public factories.
- order: source-order side effects, named reordering, once-only argument evaluation
  and omitted pure parameter defaults.
- required: explicit construction of required children in a no-init wrapper.
- numeric: all ten numeric types and Boolean defaults, boundary values and float precision.
- failure: actual initializer-body failure with source/caller trace.
- stages: argument failure before initialization, omitted-child failure and explicit
  child construction that must not execute a redundant failing default.
- copies: aggregate initializer parameters, caller independence, field assignment,
  function parameters and aggregate returns without reinitialization.

initialization_cli runs all seven in normal/O2 JIT and separately linked AOT,
three successful inputs per program (84 success comparisons/configuration), emitted
object DWARF validation and twelve failure/source-order comparisons. Existing native
value/receiver integration cases remain. Frontend checks add precise diagnostics,
required/default/private boundaries, nominal copying, pure contexts, invalid lifecycle
calls/returns, independent recovery, initializer/default navigation metadata, direct
and cyclic helper reload rejection, changed default/access contracts, trivia-safe
and unrelated-body reload and malformed initializer IR rejection.
The actual native LSP protocol suite tests imported initializer definition/references,
exact named-argument diagnostics, stale-query suppression and unsaved repair.

### Final native build/test matrices

Each final matrix below was run only after its own build completed successfully.
These include the final required-child/default/access and aggregate-copy tests.

| SDK | Tree / configuration | Result | CTest elapsed |
| --- | --- | --- | --- |
| Installed, RTTI on | ci-debug / Debug | 24/24 | 34.83 s |
| Installed, RTTI on | ci-release / Release | 24/24 | 31.31 s |
| Installed, RTTI on | ci-sanitizers / RelWithDebInfo, strict ASan/UBSan | 24/24 | 35.72 s |
| Official archive, RTTI off | ci-official-debug / Debug | 24/24 | 29.66 s |
| Official archive, RTTI off | ci-official-release / Release | 24/24 | 25.47 s |
| Official archive, RTTI off | ci-official-san / RelWithDebInfo, strict ASan/UBSan | 24/24 | 31.33 s |

The six final matrices passed **144 suite executions**. Initial concurrent runs
were slower; timings here are verification evidence, not performance benchmarks.
One earlier official Release test invocation overlapped the tail of its build:
three CLI tests encountered a temporarily absent executable during replacement.
That failed harness run is preserved in ci-official-release-overlapped-build.log;
a complete post-build rerun passed, and final build/test orchestration always
awaited a successful build before starting its own matrix. No production workaround
or test weakening was used for that scheduling mistake.

Actual commands, for each installed/official configured tree:

```sh
cmake --build build/ci-debug -j2
ctest --test-dir build/ci-debug --output-on-failure -j4
# Also ci-release, ci-official-debug and ci-official-release.
cmake --build build/ci-sanitizers -j2
ASAN_OPTIONS=detect_leaks=1:halt_on_error=1:exitcode=87 \
UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1:exitcode=86 \
ctest --test-dir build/ci-sanitizers --output-on-failure -j4
# Same strict environment/build/test for ci-official-san.
```

Verified LLJIT headers, LLVMOrcJIT library, LLVMConfig.cmake and llvm-config 19.1.7
for both /usr/lib/llvm-19 and the complete official archive under
.toolchains/ci-investigation/LLVM-19.1.7-Linux-X64. Matching Clang/LLD versions and
SDK-wide RTTI policies remain unchanged (installed YES, official NO). Fresh Debug
configuration of both SDKs passed the actual compile/link/execute ORC probe.
No LLVM version, dependency, sanitizer or leak requirement was weakened. Strict
sanitizers ran outside the filesystem sandbox so LeakSanitizer could inspect
processes. The no-RTTI archive retains its established instrumentation policy;
this does not claim unavailable vptr checks. Generated application machine code
is not itself ASan/UBSan-instrumented by the compiler/runtime sanitizer builds.
Earlier full rebuilds emitted the existing SDK ciso646/header deprecation warning;
no suppression was added. Final incremental builds had no new source warnings.

Both final Debug SDKs passed **five consecutive frontend repetitions with a 1 MiB
stack** (ten additional executions). This retains the original Phase 12 layout/
combined-expression regression, Phase 13 deep receiver/SCC guards and the new
initializer checks; limits were not raised. Both strict sanitizer SDKs passed
frontend and reload **three repetitions each** (twelve additional executions).

```sh
(ulimit -s 1024; ctest --test-dir build/ci-debug -R '^frontend$' --repeat until-fail:5 --output-on-failure)
# Same command for ci-official-debug.
# With the strict sanitizer environment above, for both sanitizer trees:
ctest --test-dir build/ci-sanitizers -R '^frontend$|^reload$' --repeat until-fail:3 --output-on-failure
```

### Extension, real editor and native executable evidence

With Node 24.19.0 and existing pinned dependencies, final `npm test` passed strict
TypeScript/build and **4/4 individually executed tests**, including the corrected
keyword assertions. A detailed TAP unit log is also retained. `npm run package`
passed and produced editors/vscode/dist/void-language-0.1.0.vsix (8 files, 145.44 KB).
No publication or native binary bundling was performed.

Real installed VS Code **1.141.0** passed trusted, restricted-trust and multi-root
profiles against native voidls, retaining actual child termination, strict
notification counts, Unicode paths, restart/configuration/fallback behavior,
no automatic restart, no child in restricted/multi-root contexts and clean shutdown.
Compiler-owned constructor navigation/revision checks additionally passed the native
LSP process suite. VSCodium and Windows/macOS editor execution were not available locally.

```sh
cd editors/vscode
npm test
node --test --test-reporter=tap out/test/config.test.js
npm run package
VOIDLS_PATH=/home/shmellyorc/Documents/Void.Language/build/ci-debug/voidls \
VSCODE_EXECUTABLE_PATH=/usr/share/code/code npm run test:host
```

Actual source-to-native demonstration from examples/initialization/main.void,
linked with matching Clang/LLD 19.1.7:

```text
Source checked successfully
166                          # normal JIT, input 10
166                          # optimized JIT, input 10
Wrote native source object: build/phase14-verification/initialization.o
166                          # separately linked native executable, input 10
```

The result is a Linux x86-64 ELF PIE with debug_info. Commands are in
INITIALIZATION.md. These results compile real imported source, not hand-built IR.
Final build/test/stress/SDK/editor/CI logs, object/executable, source hashes and
results.json are retained under ignored build/phase14-verification/ outputs.
Git whitespace validation passed.

### Remaining limits and hosted readiness

INITIALIZATION.md, the approved proposal, Bible constructor notes, README,
DECISIONS, KNOWN_ISSUES and related analysis/value/compiler docs now distinguish
working C1 from deferred features. Required-child availability and pure default
rules are deliberate approved boundaries. Runtime field defaults, general function/
method named/default parameters, initializer inference/overloads, fallible results,
resource cleanup/deinit, managed reference structs, inheritance/interfaces,
collections/strings and public aggregate ABI remain future work. The experimental
GC has not been presented as a production runtime. A coherent runtime-field-default
or fallible/resource construction design can be considered next, with owner approval;
this phase does not finalize those policies.

Phase 14 and the isolated editor assertion correction are **ready for hosted CI**.
All execution above was Linux x86-64. Windows/macOS native calling/layout behavior,
all hosted matrices and the corrected Windows editor test still require another
Actions run; Linux success is not substituted for those results. The existing
workflow discovers all 24 suites automatically. No commits, pushes, branches,
publishing or remote modifications were performed.

## Phase 14 run #21: macOS ARM64 Debug layout-cache correction (12 October 2026)

### Hosted evidence and investigation before the fix

Read-only GitHub API retrieval inspected run 38252680475 at `424cacb`, macOS job
114822713822 and artifact 11748844018 (`tests-macOS-ARM64`). The job log and
artifact's debug/Testing/Temporary/LastTest.log agree: Debug built; frontend passed
all groups through receiver methods, then aborted after `Frontend begin:
initialization` at DataLayout.h:652, `Idx < NumElements`. The other 23 suites,
including initialization_cli, passed. The run API reports Windows and Ubuntu
success. The artifact contains configuration and CTest logs, **no core/backtrace
or individual initialization-program marker**. It does not identify which of the
six original project examples was active; that hosted source cannot be recovered
from this artifact alone.

Before changing production behavior, temporarily instrumented debug_value_type
with a comparison of native->getNumElements() against
layout.getStructLayout(native)->getMemberOffsets().size(). Linux Debug caught a
real mismatch: /order.void::pair had two current fields but a cached three-field
layout. An isolated run of the original initialization group, including twenty
repetitions, passed locally; the original hosted allocation sequence was not
reproduced by that experiment.

A focused real-source reproducer retained one vd::Jit while compiling and retiring
successive init-bearing cell types through normal/O2 JIT. Field counts follow
1 + (generation * 7) % 17. The first three types have 1, 8 and 15 i32 fields.
Generation 2 constructs cell(value), whose initializer increments f14 and whose
entry returns f14. At input 10 the expected result is 24. The diagnostic caught
**native=15, cached=8** for /initialization.void::cell before execution.

Removed the temporary production guard and captured the original assertion under
GDB, before applying the fix. The unmodified lowering aborted at index 8:

```text
StructLayout::getElementOffset(Idx=8), DataLayout.h:652
StructLayout::getElementOffsetInBits, DataLayout.h:657
lower(...)::debug_value_type, backend.cpp:118
lower(... suffix="__v5"), backend.cpp:133
Jit::compile(... optimize=false), backend.cpp:417
initialization_tests
```

The debugger invocation used the temporarily isolated initialization group:

```sh
gdb -batch -ex run -ex 'bt 22' --args \
  build/ci-debug/void_tests frontend /home/shmellyorc/Documents/Void.Language
```

[LLVM 19.1.7 DataLayout.cpp](https://github.com/llvm/llvm-project/blob/llvmorg-19.1.7/llvm/lib/IR/DataLayout.cpp#L720-L739) confirms the mechanism: getStructLayout caches
by raw StructType* and returns a previous entry immediately for an existing key.
The DataLayout copy operation copies target specifications, not the layout cache.
VOID passed the persistent LLJIT DataLayout into lowering, but creates disposable
LLVMContexts per compilation. Once types are destroyed, allocator reuse can give
a new type an old pointer key and retrieve the previous type's field count,
offsets and size. A smaller cached count triggers this exact assertion; a larger
one can silently generate incorrect debug offsets. ASan need not detect this:
the cached StructLayout allocation itself is still live.

This confirms the proposed debug_value_type call site with an actual Linux
assertion/backtrace and specific source/lowering operation. It is a context/cache
lifetime defect, not an ARM64 alignment rule or a collision in VOID nominal names.
All fields in the minimal reproducer are i32; nesting and custom initialization
are not necessary for the cache defect. ARM64's allocation sequence exposes it in
a different original test group. No local macOS execution/backtrace is claimed.

### Small correction and regression coverage

Shared lowering now sets the module's target DataLayout as before, then performs
all layout queries through **module->getDataLayout()**, rather than the borrowed
LLJIT layout. This fresh cache is bounded by the module/context lifetime and
retains the identical target specification. JIT and AOT still share lowering.
No fields, indices, assertions, metadata, compiler semantics, sanitizer settings
or existing tests were removed or bypassed. Temporary production instrumentation
and the isolated-suite shortcut were removed.

The frontend initialization suite adds 64 changing field-count generations and
16 nested mixed-alignment generations, each normal/O2: 160 real source-to-native
compilations/executions on retained JITs. Nested cases alternate i8/i64 beside bool,
i32 and f64, use child/parent initializers and exercise independent copies and
aggregate parameters/returns. Existing AOT integration and DWARF verification are
retained. Program/mode and layout-generation markers now make a future hosted
abort more precise. All 24 suites and the small-stack semantic regression remain.

Evidence is retained under ignored build/phase14-macos-verification/: downloaded
hosted logs, layout-generations-before.log, assertion-backtrace.log, isolated
experiments, focused/full test logs, SDK checks and editor results. The temporary
isolation/instrumentation is not part of the delivered source changes.

### Completed local validation

Focused tests ran first after the cache correction:

```sh
cmake --build build/ci-debug -j2
ctest --test-dir build/ci-debug -R '^(frontend|initialization_cli)$' --output-on-failure
```

Both passed (frontend 4.95s; initialization_cli 2.52s). An initial version of the
new nested fixture correctly failed semantic analysis because its child initializer
required an argument; the fixture now supplies a declared parameter default, as
required by the approved recursive construction contract. No compiler rule was
changed to accommodate the test.

Then reconfigured each existing tree, rerunning the actual LLVM ORC SDK link/run
probe, built it to completion, and ran all 24 suites:

| Build tree | LLVM 19.1.7 SDK / configuration | CTest result | CTest wall time |
| --- | --- | --- | --- |
| ci-debug | installed / Debug | 24/24 | 20.05s |
| ci-release | installed / Release | 24/24 | 17.52s |
| ci-sanitizers | installed / RelWithDebInfo ASan+UBSan | 24/24 | 33.51s |
| ci-official-debug | complete official archive / Debug | 24/24 | 17.99s |
| ci-official-release | complete official archive / Release | 24/24 | 14.16s |
| ci-official-san | complete official archive / RelWithDebInfo ASan+UBSan | 24/24 | 27.20s |

Actual commands per tree were cmake -S . -B build/<tree>,
cmake --build build/<tree> -j2, then
ctest --test-dir build/<tree> --output-on-failure -j4.
Installed SDK root: /usr/lib/llvm-19 (RTTI YES). Official root:
.toolchains/ci-investigation/LLVM-19.1.7-Linux-X64 (RTTI NO). Matching 19.1.7 Clang/LLD
and complete headers, ORC library and LLVMConfig.cmake were verified; each configure
log records VOID_LLVM_ORC_RUNS success. SDK RTTI matching remains unchanged across
all linked code. Strict runs used the existing sanitizer instrumentation with:

```sh
ASAN_OPTIONS=detect_leaks=1:halt_on_error=1:exitcode=87
UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1:exitcode=86
```

No sanitizer exclusions or assertion suppression were added. The installed SDK
retains UBSan vptr checks; the official SDK retains its existing no-RTTI ABI policy.
Full suites include normal/O2 separately linked AOT, object debug verification,
source diagnostics, GC/native/hot-reload proofs and LSP/tooling tests.

Both Debug SDK builds passed frontend --repeat until-fail:5 with ulimit -s 1024
(ten executions total; 27.54s installed, 26.23s official). This preserves the
small-stack expanded-layout/resource-limit regression. The command was:

```sh
(ulimit -s 1024; ctest --test-dir build/<debug-tree> -R '^frontend$' --repeat until-fail:5 --output-on-failure)
```

Editor verification used the existing Node runtime and local VS Code:

```sh
cd editors/vscode
npm test
npm run package
VOIDLS_PATH=/home/shmellyorc/Documents/Void.Language/build/ci-debug/voidls \
VSCODE_EXECUTABLE_PATH=/usr/share/code/code npm run test:host
```

All four extension unit tests passed. Local VSIX packaging succeeded (8 files,
145.44 KB). Real editor trusted, restricted-trust and multi-root scenarios passed,
including actual native process termination, notification counts, restart and
clean deactivation. No editor production/test code changed for this correction.

Both sanitizer SDK builds additionally passed
ctest --test-dir build/<sanitizer-tree> -R '^(frontend|reload)$'
--repeat until-fail:3 --output-on-failure under the same strict environment:
three repetitions of each of two suites per SDK (twelve suite executions total;
25.41s installed, 24.71s official). No leaks or sanitizer failures were reported.

The correction is ready for the next hosted CI attempt. All new execution evidence
is Linux x86-64. The original macOS artifact cannot identify its exact source
example, and no macOS ARM64 execution is available locally; the identical assertion
was instead reproduced and backtraced on Linux with the focused source sequence.
The next macOS ARM64 Debug frontend run must confirm the repair on the originally
failing platform. Windows/macOS hosted regressions remain necessary; previous
Windows success is baseline evidence, not a test of these local changes. The
existing workflow and all 24 suites remain enabled. No language features, LLVM
version changes, commits, pushes, branches or remote modifications were made.

## Phase 15 investigation and GC runtime proposal, 12 October 2026

**Design only. Implementation is not authorized and has not started.** The owner
states Phase 14 is complete. Inspected current clean baseline `c1b4a66` and the
required Language Bible, Development Strategy, DECISIONS, KNOWN_ISSUES,
INITIALIZATION and prior WORK_LOG evidence. In particular, preserved the approved
non-moving/precise direction, value-copy and C1 initialization rules, separate GC
and code lifetimes, and the open top-level/entry-point discussion. Older pending
hosted statements remain historical; no new hosted result was inferred.

Inspected actual Heap/Root definitions and allocation/tracing, native/reload
runtime, NativeCode/ReloadSlot and compatibility, shared IR types/validation,
LLVM lowering/O2/checked failure paths, root probe, ORC contexts/resource trackers,
AOT adapter/link driver, CLI, real C fixture, concurrent invocation tests and CI.
Relevant implementation is in include/void/{heap,execution,native,backend,ir}.hpp,
src/runtime/, src/backend/llvm/backend.cpp, src/main.cpp, tests/tests.cpp and
tests/integration/. No managed source types or thread registry were found.

### Findings and recommended design

- Heap and explicit Root are single-threaded proofs. The test's thread-local
  probe_heap selector is not collector thread registration. Optimized generated
  root tests cover one stack slot, not general managed source/nested stack roots.
- Normal source IR supports numeric/Boolean and inline value types; it does not
  yet contain managed-reference allocation or safepoint operations. Native imports
  are still trusted i32(i32) proofs. AOT success does not establish GC linkage.
- Graph validation, copied layouts, iterative tracing and rollback before sweep
  are reusable. The current allocation helpers depend on collection occurring
  only at explicit probe polls: their unrooted intermediate allocations must not
  silently become an automatic-GC implementation.
- Existing leases safely retain code through active/native invocations. Preserve
  that scheme; neither GC completion nor replacement publication authorizes
  unloading an active generation. Object tracing descriptors must outlive the
  allocating generation independently; frame maps remain covered by call leases.
- The current C fixture verifies same-thread callbacks only. Its callback fields
  and resource counter do not establish foreign-thread/racing teardown support.

Created [GC_RUNTIME_PROPOSAL.md](GC_RUNTIME_PROPOSAL.md), explicitly awaiting
owner approval. It recommends per-thread explicit root frames, cooperative
Managed/Parked/NativeSafe transitions, an elected requesting collector and one
runtime metadata mutex/CV. Frozen roots and synchronized state transitions permit
tracing only when no mutator is Managed. Old-epoch notifications cannot resume a
thread during a newer request. Native waits/reentry, persistent handles, callback
provider quiescence, scoped code leases, failure cleanup and shutdown have explicit
proposed contracts. No forced suspension, background collector, alternate code
reclamation scheme, managed source syntax or speculative allocator was added.

The proposal compares explicit frames with statepoints and LLVM's built-in shadow
stack. Reviewed versioned LLVM 19 GC/Statepoints/ORCv2/LangRef documentation and
checked exact 19.1.7 installed/official SDK headers for IRBuilder statepoint/result/
relocate APIs, GCStrategy and RewriteStatepointsForGC. API presence is verified;
new statepoint lowering, stack-map consumption and thread protocols are not.
Official documentation links and the six owner decisions G1–G6 are in the proposal.

### Actual verification of the unchanged baseline

Existing build trees were used; no compiler/runtime/editor source, tests, build
configuration, sanitizer settings or workflow was edited or rebuilt for this
investigation. Both llvm-config binaries report 19.1.7. Reran only the following
existing architecture/regression suites to verify the inspected baseline:

```sh
ctest --test-dir build/ci-debug \
  -R '^(core|jit|reload|gc|native|aot|frontend)$' --output-on-failure
ctest --test-dir build/ci-official-debug \
  -R '^(core|jit|reload|gc|native|aot|frontend)$' --output-on-failure
env ASAN_OPTIONS=detect_leaks=1:halt_on_error=1:exitcode=87 \
    UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1:exitcode=86 \
    ctest --test-dir build/ci-sanitizers \
    -R '^(gc|reload|native)$' --output-on-failure
env ASAN_OPTIONS=detect_leaks=1:halt_on_error=1:exitcode=87 \
    UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1:exitcode=86 \
    ctest --test-dir build/ci-official-san \
    -R '^(gc|reload|native)$' --output-on-failure
```

| Existing configuration | Selected tests | Result | Wall time |
| --- | --- | --- | --- |
| Installed SDK, Debug | core/jit/reload/gc/native/aot/frontend | 7/7 | 5.42s |
| Complete official SDK, Debug | same | 7/7 | 4.98s |
| Installed SDK, strict ASan/UBSan | gc/reload/native | 3/3 | 1.14s |
| Complete official SDK, strict ASan/UBSan | same | 3/3 | 0.98s |

Twenty selected suite executions passed. Sanitizers ran outside the sandbox's
tracing restriction with leak detection intact; installed RTTI/vptr and official
no-RTTI coverage policies remain unchanged. Logs are under ignored
build/phase15-investigation/. These are Linux x86-64 checks of existing behavior,
not evidence for the proposed GC protocol, new foreign-thread callbacks, TSan,
performance, or new Windows/macOS execution. All 24 existing suites remain intact;
no full Phase 15 implementation matrix, small-stack or editor rerun is claimed.

### Stop for owner review

Only GC_RUNTIME_PROPOSAL.md and this log were changed. The proposal includes
acceptance tests, risks and one cohesive implementation sequence, separating the
internal typed JIT/AOT proofs from future managed-language work. Approval is needed
for G1 root representation, G2 coordination/allocation staging, G3 retention,
G4 native/callback contracts, G5 failure policy and G6 bounded implementation scope.
No implementation, commits, pushes, branches or remote modifications were made.


## 2026-10-12 — Phase 15 implementation (owner-approved G1–G6)

Implemented the six-stage sequence in GC_RUNTIME_PROPOSAL.md. This phase adds no
source managed struct, strings, async, finalizers, new CLI entry behavior or native
language feature. No commits, pushes, branches or remote changes were made.

### Runtime and safety invariants

- Added one noncopyable Runtime around the existing noncopyable Heap, one registry,
  coordinator mutex/CV, TLS attachments, Managed/NativeSafe guards, parked epochs,
  persistent handles and explicit closing. Frame publication/removal is allocation-
  free owner-thread work. Runtime allocation requires an already registered output
  root, polls before taking the heap lock, then allocates/publishes without a GC gap.
- Requesting collectors freeze themselves; competing requesters park instead of
  waiting while Managed. Tracing/sweeping holds the coordinator lock only after
  every participant is frozen. CV waits release the lock; collector code calls no
  provider, callback, ORC or application code. A waiter checks the current request
  under the lock and remains frozen if a new epoch starts before it wakes. Joining
  collectors retain the outcome/deadline of their original elected attempt.
- Deadline abort precedes tracing/sweep and releases all waiters. Root enumeration
  or worklist bad_alloc restores idle without sweeping; the existing tracer resets
  partial marks. Deterministic allocator fault injection exercises enumeration and
  partial-mark failures followed by successful recovery. Detectable corrupt roots
  or out-of-order frames terminate; isolated child tests require the terminate
  handler's exit 79 and reject ordinary continuation or sanitizer failures.
- New idle Managed entries are rejected during closing. Existing active scopes,
  NativeSafe continuations and already-admitted callbacks may finish. Cross-runtime
  attachment and Managed-only operations from NativeSafe are rejected before the
  relevant mutation. Destruction with live threads/handles/registrations is fatal.

### Shared compiler, native calls and code lifetime

- Added private nominal managed_ref and validated allocation/field/collection/poll/
  reachability operations to the existing IR. The one IR validator exposes result
  types to the one LLVM lowerer; removed the lowerer's duplicated result-type switch.
  Managed references/operations require coordinated IR; public parsing/analysis does
  not expose them. Internal schemas are bounded, nonempty and acyclic; this does
  not decide future managed source type recursion or empty-object layout.
- Generated root frames describe actual local/receiver storage and fixed parameter/
  expression spill slots. Flattened nested reference maps use the module-owned
  DataLayout, preserving the macOS correction. Slots are initialized before frame
  publication, parameters before polling, and checked exits always unlink frames.
  Entry/block polls preserve numeric-only and managed loop cooperation under O2.
  Allocation and argument/return publication use Managed no-GC handoff windows.
- JIT NativeCode retains an explicit Runtime owner plus its existing generation
  lease. Host dispatch/diagnostic copying runs NativeSafe, including reentry. Exact
  analysis contracts remain intact and are extended with conservative internal
  nominal layouts/runtime identity; changed layout/runtime bindings reject reload.
  Heap descriptors are copied numeric data independent of LLVM contexts/generations.
- Added a same-lowerer runtime object emitter for private AOT proofs; CMake links
  normal/O2 objects with the actual C++ runtime/Threads requirements. ldd on the
  installed Debug AOT executable shows libc, libstdc++, libm and libgcc, no LLVM/ORC.
  The existing CLI adapter/entry behavior is unchanged.
- Added CallbackRegistration with explicit provider unregister-and-quiesce, closing
  admission, in-flight drain, self-drain rejection, same/foreign-thread attachments
  and binding-specific failure translation. Fixture providers own/join all native
  dispatch threads, including dispatch delayed before admission. No arbitrary
  provider's teardown guarantee is assumed. Persistent captures are released only
  after quiescence. Real generated calls block in a native fixture, reenter via an
  actual callback, and retain roots/old leases while collection and reload proceed.

### Full verification — completed stable tree

Both SDKs were rechecked for exact 19.1.7, ORC headers, libLLVMOrcJIT.a and
LLVMConfig.cmake. Every configuration reran the existing real ORC configure probe.
Installed SDK: /usr/lib/llvm-19 (RTTI YES). Official complete archive:
.toolchains/ci-investigation/LLVM-19.1.7-Linux-X64 (RTTI NO). Project-wide RTTI matching
is preserved. ASan/UBSan remain enabled in both sanitizer trees; RTTI-dependent vptr
coverage is available in the installed configuration, unavailable in the archive ABI.

| SDK | Configuration/tree | Final CTest result | Test time |
| --- | --- | --- | --- |
| Installed | Debug / ci-debug | 31/31 | 20.50 s |
| Installed | Release / ci-release | 31/31 | 17.42 s |
| Installed | RelWithDebInfo + ASan/UBSan / ci-sanitizers | 31/31 | 33.82 s |
| Official archive | Debug / ci-official-debug | 31/31 | 18.21 s |
| Official archive | Release / ci-official-release | 31/31 | 13.31 s |
| Official archive | RelWithDebInfo + ASan/UBSan / ci-official-san | 31/31 | 27.23 s |

The original 24 suites and their assertions remain. Seven added suites are
runtime_coordination, runtime_generated, runtime_callbacks, runtime_failures,
runtime_aot_normal, runtime_aot_optimized and runtime_invariants. Shared normal/O2
JIT and separately linked AOT test nested aggregates, expression temporaries across
allocation, argument/return handoffs, method-created references surviving through
actual nested receiver storage, copy independence, checked failure cleanup and loop
safepoints. AOT also runs four concurrent generated callers. Numeric-only optimized
JIT loops must record actual cooperative parking. The original root/GC, reload,
native, constructor, diagnostics, tooling and frontend assertions all still run.

Commands for each of the six cached SDK/configuration trees:

```sh
cmake -S . -B build/<tree>
cmake --build build/<tree> -j2
ASAN_OPTIONS=detect_leaks=1:halt_on_error=1:exitcode=87 \
UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1:exitcode=86 \
ctest --test-dir build/<tree> --output-on-failure -j4
```

The stable-tree final run is build/phase15-verification/validate.py; results.json,
sdk.json and per-tree configure/build/test logs preserve commands/status/timings.
An earlier in-progress build was invalidated while receiver proof sources evolved
(a stale generated AOT object lacked entry_receiver). It was rebuilt; all six final
stable-tree configurations above passed. Earlier logs remain in preliminary/ and
before-native-stress/ rather than being treated as final evidence.

Additional completed checks:

- Both Debug SDKs: ctest -R '^runtime_' --repeat until-fail:10 -j4 — seven suites,
  ten executions each, all passed (20.47 s each).
- Both Debug SDKs: ulimit -s 1024; frontend repeated until-fail:5 — all five passed
  per SDK (24.40 s installed, 22.42 s official). Existing bounded-layout small-stack
  regression remains intact.
- Both strict sanitizer SDKs: ctest -R '^(runtime_.*|frontend|reload|gc|native)$'
  --repeat until-fail:3 -j4 — all 11 suites passed three times, leaks enabled
  (24.28 s installed, 22.88 s official). No sanitizer suppressions or disabled tests.
- Separate runtime-only ThreadSanitizer: clang++-19 -std=c++20 -g -O1
  -fsanitize=thread -fno-omit-frame-pointer -Iinclude tests/runtime/coordination.cpp
  src/runtime/coordination.cpp src/runtime/heap.cpp -pthread; run with
  TSAN_OPTIONS=halt_on_error=1:exitcode=85 — passed, no reported races. This check
  does not instrument the prebuilt LLVM SDK or generated application instructions.
- Release diagnostic baseline, 1000 collections with one already-NativeSafe thread
  holding one frozen object: installed mean 0.201559 us / maximum 2.28 us; official
  mean 0.135120 us / maximum 1.61 us. These fixture measurements are not production
  pause guarantees or a measure of arbitrary Managed rendezvous latency. TSan's
  separate instrumented fixture measured mean 1.62557 us / maximum 34.4 us.
- Extension npm test outside the tracing sandbox: 4/4 individual cases passed.
  npm run package: VSIX produced, eight files / 145.44 KB. A sandbox-only Node run
  reported only a file-level case; the authoritative host run executed all four.
- Real installed VS Code 1.141 Linux integration: trusted, restricted and multi-root
  modes passed; actual native-process termination, notification counts, navigation,
  diagnostics, restart and clean deactivation assertions remain intact. Expected
  injected lifecycle errors appear in its successful test log.
- git diff --check passed. AOT dependency and all extra test outputs are under
  build/phase15-verification; extra.py and extra-results.json record repetitions.

Strict sanitizer and real editor checks ran outside the tracing sandbox so leak
inspection and native editor child processes could execute normally. Requirements
and sanitizer settings were not relaxed. No hosted checks were run or remote state
changed in this phase.

### Remaining limits and next gate

Implementation is complete locally and ready for owner review/hosted CI. Windows
x64 and macOS ARM64 ABI, private root-frame calling conventions, thread scheduling
and native/editor behavior still require the owner's hosted run; Linux results do
not establish those outcomes. VSCodium/minimum-editor-version checks were not run.
Generated application instructions and prebuilt LLVM remain outside C++ sanitizer
instrumentation; architectural observations exercise those roots explicitly.

This is cooperative, nonmoving tracing with serialized allocation, frame-lifetime
retention, no allocation-triggered thresholds and no hard pause bounds. The
coordinator makes tracing safe, not arbitrary simultaneous application field writes.
General source threading/shared-mutation semantics, managed structs/construction,
provider bindings without strong quiescence, strings, async, finalizers, allocator
optimizations, tighter liveness, LTO and statepoints/stack maps remain deferred.
After hosted validation, propose the smallest managed-source type/allocation slice
using these contracts; do not infer approval of new managed language rules.


## Phase 15 run #24 macOS Release numeric-loop correction, 12 October 2026

Inspected the actual [macOS ARM64 job 115048678810](https://github.com/Shmellyorc/Void.Language/actions/runs/38320961702/job/115048678810)
logs through the GitHub read-only connector. The checkout is
`844ce7d201470e83bf6e29a2cc17dc88c41d7e21`, using the cached complete official
LLVM/Clang 19.1.7 macOS ARM64 SDK. Debug passed 31/31 (`runtime_generated`
0.65 s). Release passed 30/31: `runtime_generated` failed after 0.21 s with
`numeric loop never acknowledged a collection`. There was no rendezvous timeout
or LLVM assertion. The subsequent macOS editor steps were skipped. Windows
completed successfully; Ubuntu was still running when job status was retrieved.

### Investigation before tracked edits

The numeric-loop latch only establishes that `signal_running()` ran. The shared
lowerer wraps that import in NativeSafe guards. It establishes neither that the
worker has reentered Managed execution nor that it remains in the numeric loop
when any of the 100 collections starts. All collections can legally complete
while the worker is NativeSafe, or after it finishes. Neither requires a poll to
park. The final cumulative zero-parks assertion therefore has an invalid premise.

A scratch copy of the unchanged test, delaying collection with `worker.wait()`
after the latch, deterministically reproduced the **exact** diagnostic in both
normal and O2 modes, with no production changes. This models a legal preemption
of the collector until worker completion; it does not add a sleep or change the
iteration count. The hosted log does not identify normal versus O2 or capture
the exact interleaving, so those details cannot be claimed for macOS.

Emitted the same numeric IR with `emit_runtime_object` and examined its LLVM
19.1.7 native disassembly. In the Linux x86-64 O2 object, the backedge targets
0x50, containing calls to `vd_gc_poll` at 0x50 and 0x55, followed by the decrement,
comparison and branch back at 0x5f. Entry, initial header and return polls also
remain. Runtime polls are opaque calls with memory effects; the O2 pipeline
retains them. No missing-safepoint compiler/runtime defect was found.
Scratch reproducer, disassemblies and evidence are under `build/phase15-run24/`.
These Linux observations are not macOS ARM64 execution verification.

### Correction and regression strength

Only the test fixture and its build wiring change. The numeric loop has no native
import. A test-local poll wrapper gates the fourth generated poll (the header
reached through the first numeric backedge), while the worker is demonstrably
Managed and before the real poll acquires any coordinator lock. A separate
collector publishes its request; the observer verifies one elected collector is
Parked and both participants are attached before releasing the gate. The real
production poll must then acknowledge the pending rendezvous. The gate is
released on failure too, so joining the worker cannot strand it in test code.

CMake compiles the **existing** `src/runtime/abi.cpp` into this test executable
with only `vd_gc_poll` renamed to `vd_gc_real_poll`. The test wrapper forwards to
that identical implementation. Its ABI definitions satisfy the linker without
extracting the archive ABI object. `llvm-nm` confirms one wrapper, one real poll
and one `Runtime::current` definition. There is no fake collector, alternative
lowerer, production hook or production API change. Other executables, including
both separately linked native AOT tests, retain the uninstrumented ABI.
The test ABI object uses the same `void_options` RTTI/sanitizer requirements.

Both modes retain the original million numeric iterations, result check, 100
successful collections and positive cooperative-parks assertion. Additional
checks require exactly **2,000,003** generated polls, 100 completed epochs,
no pending request and no attached thread leaks. Per-mode evidence makes
future hosted failures distinguishable. A scratch negative control that drops
real polling from the backedge onward fails with the zero-parks diagnostic;
the gate/counting instrumentation cannot substitute for the real GC safety path.
No compiler, runtime algorithm, resource limit, assertion or sanitizer was weakened.

### Local verification after the correction

Fresh configure/build/test runs completed, with all **31/31** existing suites in
all six configurations (186 suite executions):

| LLVM 19.1.7 SDK | Debug | Release | RelWithDebInfo ASan/UBSan |
| --- | --- | --- | --- |
| Installed, RTTI ON | 31/31, 20.48 s | 31/31, 18.26 s | 31/31, 34.71 s |
| Complete official Linux archive, RTTI OFF | 31/31, 19.11 s | 31/31, 13.31 s | 31/31, 27.91 s |

For each of `ci-debug`, `ci-release`, `ci-sanitizers`, `ci-official-debug`,
`ci-official-release`, `ci-official-san`:

```sh
cmake -S . -B build/<tree>
cmake --build build/<tree> -j2
ASAN_OPTIONS=detect_leaks=1:halt_on_error=1:exitcode=87 \
UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1:exitcode=86 \
ctest --test-dir build/<tree> --output-on-failure -j4
```

Both SDKs' headers, `libLLVMOrcJIT.a`, `LLVMConfig.cmake`, exact version and RTTI
settings were rechecked; each configure executed the real ORC SDK probe.
Installed sanitizers retain vptr coverage; official sanitizers retain the matching
no-RTTI ABI correction. Leak detection and all strict error settings remain enabled.
`build/phase15-run24/validate.py`, `sdk.json`, `results.json` and the individual
configure/build/test logs preserve these results.

- Focused first: installed Release `runtime_generated` passed, then
  `ctest --test-dir build/ci-release -R '^runtime_generated$' --repeat until-fail:100
  --output-on-failure` passed all 100 (55.07 s). Official Release passed the same
  100 repetitions (51.85 s). Each execution exercises both normal and O2 JIT.
- Both Debug SDKs: all seven `runtime_` suites passed ten repetitions each.
- Both Debug SDKs: `ulimit -s 1024; ctest --test-dir build/<tree> -R '^frontend$'
  --repeat until-fail:5 --output-on-failure` passed all five per SDK.
- Extension `npm test`: 4/4 individual tests passed. Real installed Linux VS Code
  integration passed trusted, restricted and multi-root modes, including native
  child-process lifecycle assertions. `npm run package` produced the eight-file,
  145.44 KB VSIX. No extension files changed. Host/package logs are in the same
  evidence directory. Native editor and strict sanitizer checks ran outside the
  tracing sandbox; their requirements were preserved.

- Both strict sanitizer SDKs: `ctest --test-dir build/<tree> -R
  '^(runtime_.*|frontend|reload|gc|native)$' --repeat until-fail:3
  --output-on-failure -j4` passed all 11 suites three times, including leaks
  (26.48 s installed, 24.75 s official). Both Release coordination/stress fixture
  executables also passed. `extra.py`, `extra-results.json` and per-check logs
  record these additional checks.
- Final `git diff --check` passed. Only CMake test wiring,
  `tests/runtime/generated.cpp`, `docs/GC_RUNTIME.md` and this work log changed.

The correction is ready for the owner's next hosted CI attempt. Local execution
was Linux x86-64; no local macOS ARM64 or Windows execution was available. Their
build/link behavior for the test ABI object, optimized poll counts, deterministic
rendezvous and complete 31-suite matrix still require hosted confirmation. The
actual macOS failure is explained by a confirmed test synchronization defect;
its precise normal/O2 interleaving was not logged. The new test directly checks
that generated loop safepoints execute the real coordinator on those hosts.
No production compiler/runtime behavior, LLVM version, GC contract, source
language feature, commit, branch or remote state changed.

## Phase 18 implementation: approved RV1–RV7 (13 October 2026)

The owner locks Phase 17 and reports Actions run #29 passed all seven jobs with
39 suites/configuration. This is owner-reported hosted evidence, not a new remote
inspection. RV1–RV7 are explicitly approved. Work remains local: no commit, push,
branch creation or remote mutation. The seven-job workflow is unchanged.

Implemented source reference-containing value fields through the existing nominal
Type/TypeArena, typed AST, validated analysis, shared IR/LLVM and coordinated runtime.
Copies duplicate inline storage and preserve embedded object identities. Shared
finite reference paths stop at heap edges and carry per-leaf readonly facts/formal
origins. Plain/const value parameters, copies, nested fields, conversions, calls,
returns, fresh constructor profiles and receiver/storage effects preserve authority.
Inline storage writes are distinguished from referent mutation/retention. Managed
field payloads keep their declared mutable-reference capacity recursively; no
readonly laundering through existing bindings or nested storage is permitted.

C1 stays authoritative. Required references have no zero source default or implicit
allocation; no-init construction requires complete supplied fields/children. Value
init requires valid defaults before the body and infers one fixed conservative
receiver/result profile, including defaults, const inputs, helper results and joins.
S1's direct managed readiness does not extend into partial value construction.
No-init value/managed lowering shares final complete field operands. S1 staging,
once-only init, argument snapshots, checked failure and final publication remain
on the existing paths. Pure all-null templates remain authority-neutral; runtime
const views and const formals remain readonly even when null.

Independent IR checking derives leaf effects/call results and checks aggregate
build/extract/insert/store/return, receiver authority, managed storage and private
construction-place qualifiers. A CFG must-proof rejects required-reference slot
reads before initialization; null literals cannot claim nonnullable type. Existing
S1 scope/readiness/commit checks remain. LLVM translates the same logical leaf
shape into module-owned target layout offsets and checks nested nonnullable leaves
before heap commit/write. Frame/Slot homes, heap descriptors, automatic collector,
failure frames, bootstrap and hot-reload leases are reused without runtime changes.
Analysis metadata and conservative compatibility now include exact profiles/effects.

### Investigation and corrections during implementation

- The old source field gate could not simply be removed: capability analysis
  explicitly discarded aggregate reference provenance. The replacement uses the
  shared logical shape, preserving mixed leaves and constructor/helper origins.
- New invalid-source coverage exposed a real recovered-AST crash: a module constant
  `packet(owner = node())` reported the forbidden allocation but allowed the
  enclosing aggregate into constant_value. A real debugger backtrace stopped at
  compiler.cpp's evaluator indexing an empty call operand vector (then line 1767).
  Construction now propagates poisoned child types before pure evaluation. The
  negative test retains source-span and compile-time diagnostic assertions.
- Review reproduced an unsafe acceptance in the initial split-effect implementation:
  a copied readonly carrier could invoke a scalar-mutating value method on an inline
  field of its referent. Such a receiver can contain no reference leaves itself.
  Frontend and independent IR analysis now record the selected heap owner's effect
  and authority separately. Mutable execution and forged-IR/readonly-source rejection
  are both tested. No blanket readonly downgrade or runtime permission tag was used.
- S1 staging pointers correctly have no managed-reference leaf, but their private
  readonly place permission must survive fact propagation and store/call validation.
  That distinction is now explicit; whole-stage boxing/escape remains rejected.
- A new timeout test initially assumed completed_epoch counted successful sweeps.
  Inspection of Runtime::collect showed it acknowledges settled epochs, including
  aborts. The test now asserts the defined settled-epoch protocol and preserves an
  intentionally unreachable object after timeout, proving no sweep, then verifies
  worker and collector recovery. Runtime code and existing assertions were unchanged.

### New acceptance coverage

All 39 prior suite names remain. Only the obsolete managed-source assertion that
reference-containing values are deferred becomes positive acceptance. Four additive
suites bring the inventory to 43: reference_values_source, reference_values_aot_normal,
reference_values_aot_optimized and reference_values_cli.

Real examples cover copies/identity, mixed readonly leaves, fixed custom init,
pure templates, temporary referent methods, S1 aggregate fields/helpers, imports,
named argument order, compound assignments with replaced selected heap owners,
nullable failure and cyclic stress. JIT tests force collection before every
construction and also use allocation budget one. Normal/O2 source execution and
separately linked native objects use the same compiler. The native library tests
link void_runtime, not LLVM.

The stress factory returns two cyclic nodes solely through a padded nested value.
At allocation three the factory frame is gone: every live pointer slot must be an
aggregate with exactly two registered leaves, correct host-target size and independent
C++ offsetof expectations. There are no host Handles or live direct-reference slots
masking this proof. Each execution performs 1002 constructions and at least 1000
collection epochs. Failure preserves output and removes all frames, then recovery
runs on the same runtime. Deterministic semaphore/request overlap proves cooperative
parking; a separate timeout proves no sweep and recovery. A deliberately separate
host-handle test retires all code/analysis, traces the retained nested cycle using
copied descriptors, then releases it and verifies reclamation.

Source negatives cover laundering via copies/annotations/casts/parameters/returns,
existing bindings, nested/implicit managed payloads, constructor profiles and earlier
copies, required nested defaults, C1/S1 boundaries, staging-view boxing, recursive
layouts, duplicate fields and mismatches. Independent IR negatives cover discarded
aggregate qualifications, heap payload stores, receiver-dependent returns, selected
heap effects, invalid shape claims, zero nonnullable literals/storage and stage boxing.
LSP process coverage adds imported reference fields, exact UTF-16 readonly locations,
invalid-snapshot query rejection and unsaved repair. Actual JIT replacement rejects
changed aggregate capability contracts and accepts an unrelated body change.

Final focused installed-SDK Debug verification passed 8/8 (11.93 s):
`ctest --test-dir build/ci-debug -R '^(reference_values_.*|core|lsp_protocol|frontend|managed_initialization_source)$' --output-on-failure -j2`.

Both SDK checks found exact 19.1.7, headers, libLLVMOrcJIT.a and LLVMConfig.cmake.
Installed RTTI YES and official-archive RTTI NO remain matched across all targets.
Strict sanitizer flags remain address,undefined with frame pointers and no new
suppression. A first complete six-configuration 43-suite run passed during review;
its logs are archived under build/phase18-verification/pre-review. It is intermediate
evidence, not final acceptance: final code was rebuilt and the full matrix restarted
after the selected-owner/private-place audit and padded-layout regression additions.
Final matrix/stress results are recorded below when completed.

Editor verification already passed: npm test/typecheck/build, an explicit individual
unit-test run (4/4), npm run package (8 files, 145.44 KB VSIX), and real VS Code 1.141.0
integration against the built voidls (trusted, untrusted and multi-root scenarios,
including real native process termination and strict notification assertions).
VSCodium is not installed locally; Linux editor success is not hosted Windows/macOS
verification. No extension source, grammar colors or lifecycle assertions changed.

Implementation guides, approved Bible clarifications, decisions, known issues and
README now describe the implemented boundary. Richer unavailable-value initialization,
readonly nominal heap fields, managed aggregate FFI, resources/fallible construction
and unrelated language features remain deferred. Fixed root-home retention and
conservative capability joins are documented. Hosted acceptance must run all seven
configurations after owner review/push; no Windows/macOS result is inferred here.

A focused extra proof compiled a real nested required-reference value into a managed
payload (`owner(value = make(value))`), after its factory frame returned. Normal/O2
JIT and independently linked AOT passed in all six configurations (12/12 mode/build
combinations). The native harness used Runtime(1), asserted automatic collection,
result 11, and zero objects after cleanup. Exact source/harness/commands/results are
in build/phase18-verification/required.{void,cpp,py} and required-results.json;
each log's JIT stdout and native assertions were checked, not just process exit.

Promoted this source to examples/reference_values/required.void and the regular
source/CLI family. Its 257 constructions deterministically cross the current CLI
runtime budget (256) at parent publication, so future normal/O2 native CLI runs
exercise collection with the child held by the returned aggregate. This is a test
of current embedding configuration, not a new source allocation/entry policy.
The selected-owner example was also strengthened: creation homes retire in a
factory, the RHS replaces the parent edge, then 256 allocations force collection
before the caller writes its previously selected owner. Its 262 constructions and
budget-one JIT epochs are asserted. No unrelated creation frame or host handle
masks the old owner's lifetime. These are permanent regression refinements; no
compiler/runtime code changed after the completed six-configuration run. The full
matrix was rebuilt/rerun once more for the final fixture/assertion versions.

Repeated checks against the completed compiler passed before those fixture refinements:

- Each Debug SDK: 19 runtime/managed/reference-value suites repeated 10 times.
- Each Debug SDK: frontend, managed_source, managed_initialization_source and
  reference_values_source repeated 5 times with a 1024 KiB stack.
- Each strict sanitizer SDK: 23 runtime/managed/reference-value/frontend/reload/GC/
  native suites repeated 3 times, with leak detection and halt-on-error intact.
- Each Release SDK: all three managed/reference-value source suites repeated 30
  times; coordinator/latency executable also passed.
The final refined family and 1 MiB checks are reverified below; previous stress
logs remain valid evidence for unchanged compiler/runtime code and the unchanged
1002-construction cyclic carrier/overlap/failure tests.


### Final RV2/RV5/RV6/IR contract audit

Before delivery, the contract audit added explicit mutable-authority requirements
alongside mutation/retention summaries. A readonly receiver must not enter a plain
mutable helper contract merely because that helper currently reads only. The same
requirement protects independent existing mutable local capacities. Finite formal-
leaf origins stabilize first; then requirements propagate to a fixed point. Aliases
inferred from the same receiver origin, readonly getters/returns and scalar-only
methods on var copies remain valid. No readonly-mask specialization, runtime tag,
new ownership syntax or permissive plain-parameter borrow context was added.

Independent IR derives the same constraints, distinguishing initializing stores
from existing capacities/default observations. Source and forged-IR tests reject
indirect mutable helper calls; positive const-helper, local getter, same-origin
assignment and scalar-copy method tests preserve ergonomics. Return/aggregate/cast
qualification weakening is retained at IR interfaces instead of being discarded.
A separate IR test checks that an explicitly readonly result cannot become mutable.

The audit also found the absence-of-origin/null distinction: a default nullable
binding previously had an empty fact map, which could lose const-view authority.
Every nominal reference leaf now remains present even for null/default values.
Runtime const-null aliases remain readonly after var-copy and replacement; pure
nullable module templates remain authority-neutral. Both cases have regressions.

Source diagnostics now name paths such as outer.nested.owner and link the relevant
field/formal declarations, with const-helper or independent-mutable-alias hints.
The indirect-boundary regression checks that the named path and related location
are present. Ordinary source failures are diagnosed before lowering.

The earlier successful matrix/stress/editor evidence above predates this final
compile-time audit. Final acceptance rebuilds both SDKs in all configurations and
repeats the complete requested stress/small-stack/editor checks below. No runtime,
LLVM version/flags, entry policy, workflow or existing assertion was changed.

### Final acceptance results (frozen implementation)

All six final LLVM 19.1.7 configurations reconfigured, built and passed **43/43**
CTest suites, retaining all 39 previous suites and their applicable assertions.
The final focused preflight passed 8/8 in 13.05 s. No source/test/workflow input
changed between acceptance and completion: frozen-source.json verifies 228 SHA-256
inputs. Only documentation/evidence was edited afterward.

| SDK / build tree | Build configuration | Full CTest result | Test time |
| --- | --- | --- | --- |
| Installed / ci-debug | Debug | 43/43 | 63.14 s |
| Installed / ci-release | Release | 43/43 | 21.23 s |
| Installed / ci-sanitizers | RelWithDebInfo, strict ASan/UBSan | 43/43 | 82.49 s |
| Official / ci-official-debug | Debug | 43/43 | 59.20 s |
| Official / ci-official-release | Release | 43/43 | 20.37 s |
| Official / ci-official-san | RelWithDebInfo, strict ASan/UBSan | 43/43 | 76.49 s |

These are local elapsed test times under concurrent verification, not predictions
of hosted wall time. Installed SDK: /usr/lib/llvm-19 (RTTI YES). Official complete
archive: .toolchains/ci-investigation/LLVM-19.1.7-Linux-X64 (RTTI NO). Both llvm-config
report exactly 19.1.7; required LLJIT headers, libLLVMOrcJIT.a and LLVMConfig.cmake
are present, and each CMake ORC smoke check succeeds. Clang and LLD come from the
matching SDK. Sanitizer compile flags remain -fsanitize=address,undefined and
-fno-omit-frame-pointer, with SDK-matched -frtti/-fno-rtti. No suppression or reduced
sanitizer/test requirement was introduced.

The actual six-tree sequence, repeated for each tree, was:

```sh
cmake -S . -B build/TREE
cmake --build build/TREE -j2
ctest --test-dir build/TREE --output-on-failure -j4
```

Strict sanitizer environments throughout final matrix, stress and additional native
checks were:

```sh
ASAN_OPTIONS=detect_leaks=1:halt_on_error=1:exitcode=87
UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1:exitcode=86
```

Final repeated acceptance also passed, counted from individual Passed records:

| Gate | Installed | Official |
| --- | --- | --- |
| 19 runtime/managed/reference-value suites ×10, Debug | 190/190, 86.61 s | 190/190, 77.11 s |
| 4 semantic/source suites ×5, 1024 KiB stack | 20/20, 90.31 s | 20/20, 86.81 s |
| 23 strict sanitizer regressions ×3 | 69/69, 56.71 s | 69/69, 50.12 s |
| 3 managed/reference-value source suites ×30, Release | 90/90, 212.18 s | 90/90, 198.50 s |
| Coordinator/latency executable, Release | Passed, 2.03 s | Passed, 2.03 s |

That is **258 full-matrix suite executions plus 738 repeated CTest executions**,
and both extra coordinator runs. Repeated selections/commands are recorded in
all-extra-results.json and extra.py. Stack checks execute ulimit -s 1024 before
CTest; they are actual constrained processes, not an inferred limit. Repeated tests
include deterministic overlap/no-sweep timeout, GC callbacks/provider quiescence,
hot reload, failure cleanup/recovery, ordinary/O2 source compilation and linked AOT.
The additional required-publication harness was rerun against the final compiler:
**12/12** mode/build combinations passed, with exact JIT stdout 11, native budget-one
automatic GC and zero retained objects after cleanup.

Final native editor integration passed all three real VS Code 1.141.0 scenarios:
trusted (unsaved diagnostics/navigation, restart/configuration, actual process-kill
notifications and deactivation cleanup), restricted trust (no native child), and
multi-root rejection (no native child). Evidence: editor-host.log. Earlier npm test,
explicit 4/4 individual unit tests and npm run package remain current for unchanged
extension sources; the VSIX contains 8 files, 145.44 KB. VSCodium remains unavailable
locally. The extension is not bundled with the compiler/native server.

Final source demonstration, captured in demonstration.json:

```sh
build/ci-debug/voidc run examples/reference_values/copies.void 10 --no-opt
build/ci-debug/voidc run examples/reference_values/copies.void 10
build/ci-debug/void_reference_values_cli_copies_normal 10
build/ci-debug/void_reference_values_cli_copies_optimized 10
```

Every command printed **32**. The last two are actual separately linked executables
from the parsed/validated source, using void_runtime without LLVM linkage. The
cyclic aggregate-carrier test returns 2024 in normal/O2 JIT and native AOT, with
1002 constructions and no unrelated host roots masking reachability.

Final evidence is under build/phase18-verification: summary.json, results.json,
sdk.json, inventory.json, frozen-source.json, all-extra-results.json, required-results.json,
demonstration.json and per-command logs. Earlier intermediate passes are explicitly
archived in pre-review/pre-required/pre-selected-owner/pre-boundary-audit and are
not substituted for final acceptance. Verification scripts/evidence are local build
artifacts; source regressions and documentation are repository changes.

Final diff checks pass. src/runtime, tests/runtime, .github/workflows/ci.yml and
editors/vscode/src are unchanged. No commit, push, branch or remote modification.

Phase 18 is ready for architectural review and the owner's next hosted CI attempt.
All seven jobs must verify the final 43-suite inventory on Ubuntu Debug/Release/
Sanitizers, Windows Debug/Release and macOS ARM64 Debug/Release, including Debug
editor/package gates. No hosted result, Windows/macOS execution or VSCodium execution
is claimed from this Linux work. Follow-up work should review hosted acceptance and
analysis/frame pressure before separately designing broader initialization or
readonly heap-field contracts; C1/S1/P3/P4 and the documented deferred scope remain.


## 13 October 2026 — Owner-approved object-model direction (documentation only)

Updated the canonical Language Bible Section 12 to distinguish approved design
from current implementation; Section 12.1 now records single managed-struct
inheritance, explicit `virtual func` / `override func` / `super.method()`, base
substitutability, and multiple `using` fields with qualified or unambiguous
promoted access. Composition grants neither subtyping nor implicit conversion.
Section 13 records structural interface conformance through accessible methods,
including unambiguous promoted methods, with polymorphic interface calls and no
mandatory `implements`. Added the owner's inheritance/composition examples and
the three distinct intended `is` relationships.

Updated Section 65's accepted OOP decision and Section 66's open-decision summary
to separate approved rules from unresolved architectural details. `has` remains
only an OPEN future idea, not an accepted keyword or compiler requirement.
Visibility/signature rules, dispatch representation, inherited construction, GC
layout, reload compatibility, promotion precedence/nesting and initialization
order still require implementation design. All newly approved object-model
features are explicitly marked unimplemented; existing C1/S1/P3/P4 and locked
Phase 16–18 semantics are unchanged.

Validation: reviewed the documentation diff and Markdown fences, checked the
requested design statements and examples, and passed `git diff --check`. Only
`docs/LANGUAGE_BIBLE.md` and `docs/WORK_LOG.md` changed. No compiler, runtime, test,
build or workflow files changed; no executable tests were needed or run for this
documentation-only update. No commit, push, branch or remote modification.

## 13 October 2026 — Phase 19 composition investigation (design only)

Owner baseline: Phase 18 is LOCKED; GitHub Actions run #30, attempt #2 passed all
seven configurations with 43/43 suites each. This hosted evidence is owner-reported,
not a new hosted run inspected or performed during this investigation.

Created COMPOSITION_PROPOSAL.md for approval, following canonical Bible Sections
12.1/13 and current Phase 16–18 implementation guides. No Phase 19 implementation
is authorized or included. Recommended U1–U7: multiple named nonnullable nominal
using fields on managed/value owners; direct-member precedence in one selection
namespace; ambiguity rather than signature-based selection; initially one-hop
promotion with nested qualified access; unchanged declaration ownership, access and
method import rules; ordinary stored-field construction/defaults; resolved-path
reuse with shared capability, initialization, tooling and compatibility analysis.
Automatic transitive promotion is explicitly proposed for deferral, not prohibited
permanently. These details require owner approval and do not silently amend the Bible.

Actual source investigation traced Parser fields, nominal layout/method registration,
Semantics field_expression/method/assignment, receiver-place lowering, capabilities.inc,
construction.inc, independent IR path/capability checks, target-layout root offsets,
GC runtime contracts, shared analyzed references and tooling comparison. Concrete
gaps: no using marker or shared promotion selector; separate read/write/call lookups;
assignment navigation assumes one written field token per physical field index.
Existing receiver-place and selected heap-owner snapshots support qualified mutation;
promotion must elaborate to those paths before readonly/effect/readiness analysis.
No extra runtime object, forwarding method, IR opcode, GC root system or backend is
recommended. C1/S1/P3/P4 and RV1–RV7 remain authoritative. The proposal covers required
negative cases, realistic proposed source examples with expected results, alternatives,
implementation stages and full future implementation/hosted acceptance gates.

Focused unchanged-baseline verification (existing Debug build artifacts; no rebuild):

```sh
ctest --test-dir build/ci-debug -N
ctest --test-dir build/ci-debug -R '^(reference_values_source|managed_initialization_source|receiver_methods_cli|lsp_protocol)$' --output-on-failure
ctest --test-dir build/ci-official-debug -R '^(reference_values_source|managed_initialization_source|receiver_methods_cli|lsp_protocol)$' --output-on-failure
```

Inventory: **43 suites**. Installed SDK Debug: **4/4 passed**, 11.76 seconds.
Official SDK Debug: **4/4 passed**, 10.90 seconds. Both runs exercised real existing
source, receiver CLI and LSP tests. Installed cache uses /usr/bin/clang++-19 and
/usr/lib/llvm-19/lib/cmake/llvm; official cache uses the matching compiler/LLVM
package in .toolchains/ci-investigation/LLVM-19.1.7-Linux-X64. These are baseline
mechanism checks, not proof of Phase 19 syntax, promotion or new runtime behavior.
No Release/sanitizer/full matrix/editor packaging run was necessary for this design
investigation. No Windows/macOS execution is claimed.

Documentation validation: reviewed proposal consistency, Markdown fences and local
source/document references; git diff --check passed. Only COMPOSITION_PROPOSAL.md
and WORK_LOG.md changed. Compiler/runtime/tests/build/workflows and locked Bible
semantics remain unchanged. No commit, push, new branch or remote modification.
Stop here for architectural review and owner approval before implementation.


## 13 October 2026 — Phase 19 named/unnamed composition design amendment

The owner approved the direction of U2–U7 and amended U1 before final implementation
approval. Updated COMPOSITION_PROPOSAL.md and canonical LANGUAGE_BIBLE.md Sections
12.1, 65 and 66 consistently. Both `using motion: movement` and `using movement`
embed ordinary storage; the latter derives stored name movement and qualified
access object.movement. Both forms promote identically. Duplicate stored names,
including implicit/explicit collisions, diagnose at declaration; explicitly named
fields distinguish repeated embeddings of one type.

Confirmed the owner-approved distinction: components may legally share member
names, while ambiguous promoted access diagnoses at compile time at the access.
Member types (including i32 versus f32), argument signatures and expected result
types cannot guess a component. Qualification selects the intended component
subject to ordinary access/type checks. Direct-member precedence and one-hop
promotion remain unchanged. Method-only reuse is a separate deferred question;
no stateless trait/mixin behavior is introduced. Updated examples, parser/analysis
approach and future acceptance cases for shorthand and ambiguity accordingly.

Preserved exact method ownership/imports, C1/S1/P3/P4/RV1–RV7 and all locked phases.
U1–U7 now reflect the amendment and are returned for final implementation approval;
this task grants no implementation authorization. Historical investigation results
above remain baseline evidence, not proof of new syntax. No executable tests or
build matrix were run for this documentation-only amendment. Documentation/fence
and consistency checks plus git diff --check passed. Only the three requested
documents changed during this amendment; preexisting proposal/work-log investigation
changes were preserved. No compiler/runtime/tests/build/editor/CI changes, commits,
pushes, new branches or remote modifications.

## 13 October 2026 — Phase 19 approved composition implementation

The owner approved revised U1–U7, including named/unnamed stored composition,
following the investigation/amendment above. Phase 18 remains locked at the
owner-reported run #30 attempt #2 (seven jobs, 43/43 suites). Implemented through
the existing frontend and shared pipeline; no inheritance/interface/virtual,
transitive promotion, has or stateless mixin behavior was added.

Parser fields now carry a using marker. Both `using motion: movement` and
`using movement` resolve to ordinary named physical fields, including imported
qualified type shorthand. Contextual `using: movement` remains an ordinary field.
Nonnullable nominal managed/value targets retain original layout/default rules;
primitives/nullable targets diagnose. Duplicate physical names diagnose at declaration.

One member selector supplies direct-first lookup, owner/import/private checks,
one-hop candidates and ambiguity independently of member/argument/result typing.
Overlapping promoted names remain legal until access. Related diagnostics identify
qualified routes and actual declarations. Field reads and receiver calls elaborate
to existing typed field chains; assignment paths expand separately from written
reference spans. Contextual numeric anchors use the same selector. Receiver calls
select the original embedded storage; original owner snapshots and source order
are retained through allocations and RHS/argument replacement effects.

Existing reference-leaf effects and staged construction analysis consume these
physical paths unchanged. C1/S1/P3/P4/RV1–RV7 remain authoritative. Added using-field
metadata and implicit dependencies to shared validated analysis, navigation and
exact compatibility/constant/layout records; tooling compares the marker. Marker
changes reject reload even with identical physical layout. Compatible ordinary
body edits still reload. No IR opcode, verifier exemption, LLVM storage/GC path,
allocator, runtime API or forwarding method was introduced. Production runtime,
independent IR validator, LLVM backend and seven-job CI workflow are unchanged.

Four new suites (composition_source, composition_aot_normal,
composition_aot_optimized, composition_cli) extend **43 to 47**, retaining all old
suites/assertions. New source checks cover both forms, qualification, one-hop depth,
direct precedence, duplicate names, field-type/method-signature ambiguity, exact
imports/private methods, copies, readonly returns/storage, required staged fields,
custom init, contextual wide literals, real navigation and reload. Independent IR
negative tests forge receiver paths/effects, readonly selected-owner authority and
initializer markings. Six actual source fixtures plus imported modules cover main,
stress, imports, order, staged and receiver_order in normal/O2 JIT and linked AOT.
GC proofs inspect aggregate-only roots without host handles, force budget-one and
pre-allocation collection, test deterministic rendezvous/no-sweep timeout, allocation
failure cleanup/recovery and selected old owners after containing-reference replacement.

The real LSP protocol retains old assertions and adds imported promoted read/write/
method navigation, ambiguity with related locations, invalid-query suppression and
unsaved repair. Editor grammar adds contextual using with existing keyword colors;
new assertions preserve ordinary using identifiers and excluded future keywords.
Extension lifecycle code is unchanged. Implementation guide COMPOSITION.md and
proposal/Bible/source/value/managed/reference guides, decisions, limitations and
README distinguish the working source surface from deferred scope.

Initial focused checks passed, including original frontend regressions. Two new
test-fixture issues were corrected without changing production semantics: a string
replacement accidentally qualified an already qualified path twice, and an LSP
query selected a local argument rather than the intended field. Unknown-method
lookup retains the original diagnostic code. New IR mutations target the actual
owned method source name, ensuring the negative proof really changes its effect.

During initial sandboxed sanitizer builds, both SDKs stopped when generated-source
compiler processes reached LeakSanitizer: "LeakSanitizer does not work under ptrace".
This is an execution-environment limitation, not a diagnosed source memory failure.
Reran strict sanitizer build/test commands outside the traced sandbox with leak
detection intact; final results below supersede those incomplete attempts. No ASan,
UBSan, vptr, leak setting, assertion or required test was disabled or suppressed.

Final review retained the original type-mismatch diagnostic category for nullable
field assignment after consolidating lookup; a new regression explicitly checks
it. The final six-configuration acceptance run includes that correction and the
additional independent readonly/initializer IR negative proofs. The shared source
test helper now recursively tracks imported example .void dependencies; all six
build trees are audited so changing lib/movement.void or lib/wrapper.void cannot
leave a stale linked AOT fixture. This dependency-only adjustment changes no LLVM
optimization or runtime configuration. Separate dependency-audit logs record
reconfiguration/builds for trees configured before the helper adjustment.

### Final Phase 19 acceptance evidence

Final source/test/tooling inputs were frozen and checked across validation:
**251 files**, recorded in build/phase19-verification/frozen-source.json. SDK
headers, libLLVMOrcJIT.a and LLVMConfig.cmake exist; LLVM, Clang and LLD are all
**19.1.7** in both installed and official SDKs. Installed SDK reports RTTI YES;
official archive reports RTTI NO. Existing matching compilation/RTTI policy remains
intact. Sanitizer configurations retain RelWithDebInfo plus VOID_SANITIZERS=ON,
strict ASan/UBSan and leak detection, without exemptions.

Final six-configuration matrix, all **47/47**:

| SDK | Configuration / build tree | CTest seconds |
|---|---|---:|
| Installed | Debug / ci-debug | 63.73 |
| Installed | Release / ci-release | 22.68 |
| Installed | Strict sanitizers / ci-sanitizers | 100.63 |
| Official archive | Debug / ci-official-debug | 60.79 |
| Official archive | Release / ci-official-release | 19.30 |
| Official archive | Strict sanitizers / ci-official-san | 99.24 |

Actual final matrix commands, run for each tree with its existing matching cache:

```sh
cmake -S . -B build/TREE
cmake --build build/TREE -j2
ctest --test-dir build/TREE --output-on-failure -j4
```

The orchestrator is build/phase19-verification/accepted-validate.py; per-tree
configure/build/test logs and result JSON files are under accepted/. The final
run executed outside the traced sandbox so LeakSanitizer could enumerate threads.
Environment for sanitizer build-time compiler execution, CTest and repetitions:

```sh
ASAN_OPTIONS=detect_leaks=1:halt_on_error=1:exitcode=87
UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1:exitcode=86
```

No tested assertion changed to accommodate failures. Initial/intermediate results
in the top-level and final/ directories, and pre-final-extras/, are historical;
they are not substituted for accepted/ and the final extra-result files. Additional
dependency-audit reconfigure/build logs verify the recursive imported-source graph;
all six native import targets depend on both lib/movement.void and lib/wrapper.void.

Final repeat commands (build/phase19-verification/extra.py):

```sh
ctest --test-dir build/TREE -R '^(runtime_.*|composition_.*|managed_source|managed_initialization_source|reference_values_source|reload|gc)$' --repeat until-fail:5 --output-on-failure -j2
# Both sanitizer trees use until-fail:3, with the strict environment above.
ulimit -s 1024
ctest --test-dir build/DEBUG_TREE -R '^(frontend|managed_source|managed_initialization_source|reference_values_source|composition_source)$' --repeat until-fail:3 --output-on-failure
```

Each Debug/Release SDK run passed **80/80** repeated executions (16 suites x 5);
each sanitizer SDK passed **48/48** (16 x 3). Each Debug SDK passed **15/15**
1 MiB-stack executions (5 suites x 3), including the original bounded frontend
semantic regression. Total additional final executions: **446**. Full matrices
contribute 282; final CTest acceptance total is **728**, all passed. GC repetition
covers genuine deterministic collection overlap, no-sweep timeout, callbacks,
failure/recovery, cycle reclamation, aggregate-only roots and selected owners.

Editor/tooling acceptance using Node **24.19.0**, npm **11.6.2**, VS Code **1.141.0**:

```sh
cd editors/vscode
npm test
node out/test/config.test.js
npm run package
VOIDLS_PATH=/home/shmellyorc/Documents/Void.Language/build/ci-debug/voidls VSCODE_EXECUTABLE_PATH=/usr/share/code/code npm run test:host
```

The local Node/npm locations were added to PATH; no dependency/version changes.
npm test passed; direct individual unit execution passed **4/4**. Packaging passed:
**8 files, 145.46 KB**. Actual editor integration was rerun against the final built
server and passed all **three** scenarios: trusted diagnostics/navigation with
unsaved repair, strict process-kill notifications, restart/configuration and clean
deactivation; restricted trust without native spawn; multi-root rejection without
native spawn. Existing lifecycle assertions remain intact. Evidence: editor-unit.log,
editor-individual-unit.log, editor-package.log and editor-host-final.log. Native
LSP composition navigation/ambiguity/repair passed in each full matrix. VSCodium
is not installed and was not executed locally.

Final real source demonstration (demonstration.json):

```sh
build/ci-debug/voidc run examples/composition/main.void 10 --no-opt
build/ci-debug/voidc run examples/composition/main.void 10
build/ci-debug/void_composition_cli_main_normal 10
build/ci-debug/void_composition_cli_main_optimized 10
```

All four print **111**. These compile actual source: the last two execute separately
linked native objects with void_runtime and platform dependencies, without LLVM
linkage. Source fixtures also prove result 103 for receiver replacement/selection
and 2024 for aggregate-carried cycles with 1002 constructions at input 10.

build/phase19-verification/summary.json aggregates exact commands/results/counts;
inventory.json proves every original 43 suite remains and exactly four were added.
preserved.json verifies production runtime, runtime tests, independent IR verifier,
LLVM backend, seven-job workflow and extension lifecycle sources are unchanged.
Final source hashes and Markdown/diff checks pass. No commit, push, branch or remote
modification occurred. Build evidence/scripts are local ignored artifacts.

Phase 19 is ready for owner review and the next **seven-job hosted CI** gate:
Ubuntu Debug/Release/Sanitizers, Windows Debug/Release and macOS ARM64 Debug/Release,
with 47 suites per job and Debug editor/package gates. No hosted result or local
Windows/macOS execution is claimed. Remaining scope is deliberate: one-hop only;
nonnullable nominal using targets; existing owner imports and visibility; no
inheritance, interfaces, virtual dispatch, transitive promotion, has or stateless
mixins. C1/S1/P3/P4/RV1–RV7 and all locked phases remain authoritative. Follow-up
should first review hosted acceptance, then design any additional composition or
dispatch behavior separately rather than silently expanding this phase.

## 13 October 2026 — Phase 19 run #32 Windows CRT linkage correction

Investigated immediately without waiting for the other five jobs. Retrieved the
actual logs for [run #32](https://github.com/Shmellyorc/Void.Language/actions/runs/38445187406):
Windows Debug job **115461612050**, Windows Release job **115461611926**.
Local baseline commit: **f14724b**, Phase 19 composition implementation. Both jobs
successfully configured and failed during Build while linking
void_composition_cli_imports_normal.exe, before CTest could run. Log copies are in
build/phase19-windows-crt/windows-debug.log and windows-release.log.

### Diagnosis and evidence

Both real link commands invoke LLVM 19.1.7 lld-link directly through CMake's
vs_link_exe. Direct inputs are composition-cli-imports-normal.obj, void_runtime.lib,
legacy_stdio_definitions.lib and standard Windows system import libraries. The
command contains no CRT library, and the generated object has no host C/C++ compiler
runtime-selection directives. Errors include mainCRTStartup, __chkstk, strtoll,
puts and legacy_stdio_definitions' __acrt_iob_func/UCRT dependencies. The legacy
library itself was found; these are undefined symbols, not missing SDK file paths.
Debug adds /debug /INCREMENTAL; Release uses /INCREMENTAL:NO. Neither difference
supplies CRT selection.

Compared the failing target with successful native targets from the SAME Windows
logs: managed/reference-value CLI executables had already linked; composition main,
stress and normal AOT harness targets also linked. They use the same helper/runtime
policy. The distinguishing property is that imports.void and both imported modules
contain only value structs. Shared frontend lowering marks a project coordinated
when it contains a managed declaration; this project remains uncoordinated. Its
native object therefore needs no vd_runtime_entry/GC function. No member of the
static void_runtime archive is extracted to satisfy those functions, so none of
that archive's host-compiled /MD directives contributes CRT selection. A static
library's presence on a link command does not load every member/directive.

Corroborating actual local object inspection with LLVM 19.1.7 llvm-nm:
composition-cli-imports-normal.o has exactly printf, puts and strtoll undefined;
composition-cli-main-normal.o additionally needs vd_runtime_entry, vd_gc_construct,
frame enter/leave and polling. Existing successful managed executables incidentally
obtained CRT directives through extracted runtime members. Native AOT harnesses
also have directly compiled C++ sources. The new runtime-free CLI exposed a latent
shared CMake executable-model problem, not a need to make value programs managed.
CMAKE_MSVC_RUNTIME_LIBRARY=MultiThreadedDLL correctly selects /MD for compilations
in both Windows configurations; LINKER_LANGUAGE=CXX alone does not synthesize an
MSVC object's directives when an executable contains only an external object.

Primary references supporting the toolchain behavior:
[CMake MSVC_RUNTIME_LIBRARY](https://cmake.org/cmake/help/latest/prop_tgt/MSVC_RUNTIME_LIBRARY.html),
[Microsoft /MD runtime selection](https://learn.microsoft.com/en-us/cpp/build/reference/md-mt-ld-use-run-time-library?view=msvc-170).
The diagnosis also rests on actual logs and the independent object-format experiment,
not documentation alone.

Ran a focused cross-target COFF experiment with the complete official Linux-hosted
LLVM **19.1.7** llc, clang-cl, llvm-lib, llvm-readobj and lld-link, without changing
production compiler/runtime code. A minimal generated-style Windows main referencing
puts/strtoll and a stack frame produces mainCRTStartup/__chkstk/puts/strtoll failures
when paired only with an UNUSED /MD-compiled archive. Its COFF object has no runtime
directives. Making an archive member needed selects MSVCRT. Compiling the actual
new host translation unit with /MD and Debug (/Od /Zi) or Release (/O2) flags emits
**/DEFAULTLIB:msvcrt.lib /DEFAULTLIB:oldnames.lib** in both cases; directly linking
that object also selects MSVCRT independently of archive extraction.

This is a real COFF/directive/archive-extraction reproduction, **not a successful
Windows CRT link or Windows execution**. The Linux host has no Microsoft Windows
SDK/CRT import libraries; the after-selection experiment consequently stops at
missing msvcrt.lib/oldnames.lib. No fake CRT implementation or Wine execution is
substituted for hosted Windows validation. Script, exact commands, outputs and
assertions are in build/phase19-windows-crt/reproduce.py and coff/results.json.

### Small shared correction and regression coverage

The existing void_managed_source_tests CMake helper now adds
**tests/integration/native_link.cpp** as a directly compiled source to every
source-generated CLI executable in all four test families and both LLVM modes.
It intentionally defines no symbols or entry point. CMake compiles it with the
executable's existing runtime/ABI/usage requirements, letting the real host compiler
supply the configured CRT directives. Generated VOID main remains authoritative.
There is no hardcoded CRT library/runner path, forced archive extraction, custom
entry point, linker flag workaround, runtime bootstrap change or alternate compiler.
The existing release dynamic MSVC runtime policy remains the same in Debug and
Release, matching LLVM's SDK; legacy_stdio_definitions remains required.

Regression coverage stays within the existing **47 suites**:

- composition_source explicitly asserts imports.void remains runtime-free. This
  prevents future managed declarations/bootstrap dependencies from accidentally
  masking the linker regression by extracting a runtime archive member.
- composition_cli keeps all existing normal/O2 JIT/AOT assertions and additionally
  runs both pure-value native imports executables with default input (stdout 2)
  and malformed/out-of-i32-range input (exit 2 and the exact expected puts message).
  Alongside existing valid argument/printf cases, these exercise generated main,
  strtoll, puts and printf with no managed runtime dependency. The same native
  targets/tests must build and execute under both Windows Debug and Release.

No compiler, runtime, frontend, IR/LLVM behavior, source fixture, editor or workflow
was changed. U1–U7, C1/S1/P3/P4/RV1–RV7 and all existing assertions remain intact.
Only the common test-build helper, host link translation unit, two regression files
and this work-log entry are changed.

Focused configure/build passed first for composition_source and the imports normal/
optimized plus managed main native targets. Focused CTest passed **2/2**, 5.91 sec.
Full six-configuration and applicable regression results follow below; local Linux
success will not be described as verified Windows linking.

### Final local validation of the CRT correction

Both complete LLVM/Clang/LLD **19.1.7** SDKs were verified, including ORC headers,
libLLVMOrcJIT.a and LLVMConfig.cmake; installed RTTI YES and official archive RTTI
NO retain their existing matching policy. The complete six-configuration Linux
configure/build/CTest matrix passed, with every original suite retained:

| SDK | Configuration / tree | Result | CTest seconds |
|---|---|---|---:|
| Installed | Debug / ci-debug | 47/47 | 67.21 |
| Installed | Release / ci-release | 47/47 | 24.50 |
| Installed | Strict sanitizers / ci-sanitizers | 47/47 | 83.64 |
| Official archive | Debug / ci-official-debug | 47/47 | 61.09 |
| Official archive | Release / ci-official-release | 47/47 | 21.12 |
| Official archive | Strict sanitizers / ci-official-san | 47/47 | 77.43 |

Actual commands for each existing matching build cache:

```sh
cmake -S . -B build/TREE
cmake --build build/TREE -j2
ctest --test-dir build/TREE --output-on-failure -j4
```

The matrix ran outside the tracing sandbox so LeakSanitizer could enumerate threads;
strict settings were preserved for build-time compiler invocations and tests:

```sh
ASAN_OPTIONS=detect_leaks=1:halt_on_error=1:exitcode=87
UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1:exitcode=86
```

No sanitizer failure was suppressed. All 282 full-matrix CTest executions passed,
including GC coordination/callback/failure, hot reload, frontend initialization/
capabilities, native/JIT/AOT and real LSP protocol regressions. Both normal/O2 native
imports executables built and passed the new default/malformed/range CRT checks in
every configuration. The existing native printf interoperability tests remain.

Additional focused repetition passed on both Debug SDKs:

```sh
ctest --test-dir build/DEBUG_TREE -R '^composition_cli$' --repeat until-fail:10 --output-on-failure
ulimit -s 1024
ctest --test-dir build/DEBUG_TREE -R '^(frontend|composition_source)$' --repeat until-fail:3 --output-on-failure
```

Results: **20/20** native-CLI repetitions and **12/12** 1 MiB-stack executions.
These retain the original bounded frontend regression and Phase 19 source/GC proofs.
Extension sources, lifecycle code, compiler/server and workflows did not change;
no extra editor packaging/GUI rerun was necessary for this test-build-only correction.
The real LSP suite was rerun in all six matrices.

Evidence is under build/phase19-windows-crt: actual Windows logs; object-comparison.json;
coff/ and reproduce.py; focused configure/build/test logs; matrix/ per-tree logs and
result files; native-CLI repetition results; small-stack logs; inventory.json,
frozen-source.json and summary.json. Final hashes verify the tested implementation/
regression inputs. git diff --check passed. Diff inspection confirms no changes
under src, include, examples, editors, cmake or .github, and no existing test/suite
was removed or relaxed. Only CMakeLists.txt, tests/integration/native_link.cpp,
tests/composition/tests.cpp, tests/integration/composition.cmake and WORK_LOG.md changed.

**Windows Debug/Release success remains unverified.** The next owner-pushed commit
must build the corrected imports normal AND optimized executables with the official
Windows LLVM SDK and real MSVC/Windows CRT libraries, then pass all 47 suites in
both Windows jobs. The other five hosted jobs should retain their usual gates;
none was awaited or used as a substitute for the Windows investigation. The local
COFF selection proof is not a Windows execution result. No commit, push, branch or
remote modification occurred. The correction and local validation are complete
and ready for owner review/push.

## 13 October 2026 — Phase 20 inheritance/virtual dispatch investigation only

The owner reports Phase 19 LOCKED: Actions run #33, attempt #2 passed all seven
jobs. This supersedes the preceding pending Windows gate for the CRT correction;
it is owner-reported hosted evidence, not an independently fetched run in this
design task. All **47 suites**, the shared native link input and U1–U7 remain intact.

Read canonical Bible Sections 12/12.1/13 and relevant visibility/construction/
readonly decisions, strategy, composition and Phase 14/16–18 implementation guides,
GC runtime, decisions, limitations and current work-log evidence. Inspected actual
Type/ValueType/FieldType/Place/Instruction representations; frontend declaration,
member lookup, construction and reference-capability analysis; independent IR
verification; LLVM payload layout, roots, receiver calls, debug metadata, JIT/AOT
bootstrap and generation ownership; Heap/Runtime descriptors, publication/locks,
ABI entry helpers, reload leases and shared tooling/navigation comparisons.

Concrete gaps: no base edge/subtyping, inherited field ownership, virtual slot,
dynamic object identity or descriptor-admission bootstrap. Heap metadata currently
copies size/reference offsets only. Calls/effects and constructor plans assume one
nominal callee/field set. The source AOT helper creates Runtime and enters Managed
without class admission. JIT replacement retains a whole coherent generation and
reuses Runtime; there is no proven per-method patching/vptr lifetime mechanism.
These existing facts guide the proposal rather than assuming planned features exist.

Created **INHERITANCE_PROPOSAL.md**, recommendations **I1–I8**, for owner review.
Recommended architecture: one complete managed identity/payload, full base payload
at offset zero without tail-padding reuse; original owned logical fields plus one
shared physical-layout mapper; authority-preserving explicit IR upcasts; immutable
code-independent runtime nominal/tracing metadata; generation-owned guarded slot
dispatch functions using the existing direct call/failure/root pipeline. The
readonly/effect contract conservatively joins all possible overrides. Objects hold
no method pointers or code-generation leases. Existing NativeCode/resource trackers
remain authoritative; new entries use new dispatch on old objects while old active
calls retain their coherent old graph. Exact-key generation catalogs avoid mutable
per-runtime IDs in shared AOT globals. The design extends existing runtime/IR
structures, not the heap, tracer or language type system in parallel.

New public decisions are explicitly recommendations, NOT approved/implemented:
managed upcast/nullability/equality scope; inherited visibility/imports and name
hiding; exact override signatures and inferred family effects; lookup relative to
using; mandatory explicit super.init prelude and own derived init for init-bearing
ancestry; base/default/body order; and rejection of virtual-family calls on staged
self/aliases. Proposed base-prefix construction views retain no managed identity,
use bounded existing helper contexts and preserve S1 required-field readiness.
Every initializer runs once in its phase; one most-derived commit follows success.
The prelude is clearly flagged as a new owner-level exception to direct-init
rejection. Completed child objects may dispatch during init; unfinished self may
not. All C1/S1/P3/P4/RV1–RV7 invariants remain mandatory, with alternatives recorded
instead of silently fixing new source semantics.

The proposal compares object-held vptrs, stable runtime cells, generation-local
function tables and guarded switches, plus prefix/flat/separate-object layouts.
It includes prospective source/results, rejected cases, source/independent IR
negative tests, descriptor/bootstrap/failure and lease proofs, LSP/reload contracts,
a cohesive incremental sequence and complete six-Linux/seven-hosted acceptance
gates after approval. Protected access, downcast/pattern expansion, covariance,
constructor inheritance, open-world loading, interfaces, value/multiple inheritance,
transitive promotion and stateless mixins remain outside the recommended slice.

Useful unchanged-baseline checks only (existing builds, no production rebuild):

```sh
ctest --test-dir build/ci-debug -R '^(composition_source|managed_initialization_source|reference_values_source|reload|lsp_protocol)$' --output-on-failure
ctest --test-dir build/ci-official-debug -R '^(composition_source|managed_initialization_source|reference_values_source|reload|lsp_protocol)$' --output-on-failure
ctest --test-dir build/ci-debug -N
```

Installed LLVM 19.1.7 Debug: **5/5**, 12.00 sec. Official LLVM 19.1.7 Debug:
**5/5**, 11.58 sec. Inventory: **47 suites**. These prove existing mechanisms still
pass; they do not prove inheritance/virtual implementation. Logs are under
build/phase20-investigation/{installed,official}-baseline.log and inventory.log.
No full build matrix, strict-sanitizer run or GUI packaging rerun was necessary
for this documentation-only investigation.

A focused target-layout experiment used the official **LLVM 19.1.7 llc**:

```sh
.toolchains/ci-investigation/LLVM-19.1.7-Linux-X64/bin/llc -mtriple=TRIPLE build/phase20-investigation/layout.ll -o build/phase20-investigation/TRIPLE.s
```

For x86_64-pc-linux-gnu, x86_64-pc-windows-msvc and aarch64-apple-darwin alike,
base {i64,i8} size is **16**, flat appended child byte offset is **9**, embedded
base-prefix child byte offset is **16**. This validates the tail-padding concern
and motivates the full-prefix recommendation. Initial assembly-value extraction
read the leading zero in llc's `0+16` expression; the evidence extractor was
corrected to parse the complete constant, with no production change. Final assembly
and layout-results.json retain the real emitted constants. Cross-target layout
emission is NOT Windows/macOS execution or VOID inheritance acceptance.

Documentation/fence/diff checks pass. Only docs/INHERITANCE_PROPOSAL.md and this
work log changed; compiler/runtime/tests/build/editor/workflows and Bible are
unchanged. No commit, push, branch or remote modification occurred. Implementation
is not authorized by this task: STOP for owner review/approval of I1–I8.

## Phase 20 implementation — managed inheritance and explicit dispatch (13 October 2026)

The owner approved I1–I8 after the investigation above. Implemented through the
existing frontend, nominal model, capability/readiness analysis, shared IR and LLVM
backend. No commits, pushes, branches or remote changes were made. The seven-job
workflow, LLVM 19.1.7 pins, RTTI correction, strict sanitizer options and Phase 19
Windows direct-input CRT correction remain intact.

The source surface includes managed-only single extends, identity-preserving
ancestor conversions/related equality, exact virtual/override families, direct
super calls, inherited storage/member ownership and approved using precedence.
Initializers are not inherited: a derived init has a dedicated first super.init
prelude. Init-free ancestry keeps named inherited/own fields. One private full stage
prepares base-to-derived and publishes once. Required-reference readiness, C1 value
entry validity, readonly leaf capabilities and nonescaping helpers remain binding;
unfinished virtual dispatch/identity/escape/rebinding and prefix publication reject.
INHERITANCE.md records the implemented contract; the Bible/decisions and relevant
implementation guides distinguish it from deferred interfaces/downcasts/etc.

One physical field mapper preserves the complete base payload, including padding,
for field accesses, root offsets, stage storage, descriptors and debug layout.
Runtime descriptors own exact nominal keys, ancestry and complete tracing shape,
with transactional NativeSafe admission. Heap objects contain no code addresses or
LLVM type/catalog pointers. Generation-owned guarded switch dispatch queries the
existing heap, releases its short lock, then calls a verified implementation. AOT
admits the same immutable metadata before Managed entry. No source entry policy,
second allocator, conservative tracer, forwarding-method subsystem or per-method
patch mechanism was introduced.

Review strengthened independent invariants with targeted regressions:

- Base completion tracks may/must state across joins, rejecting repeated preludes
  on backedges. An alias to a Base prefix cannot publish a separate Base identity;
  final commit must match the owning most-derived stage.
- Cached layouts cannot bypass the combined inheritance/inline physical depth of
  128; expanded fields remain capped at 65536. Declaration-order-independent and
  mixed inheritance/value-depth source cases diagnose before LLVM lowering.
- Every virtual receiver belongs to its nominal catalog; registered nominal source
  objects cannot bypass complete construction through legacy raw allocation.
  Legacy untyped experimental IR proofs remain unchanged and untyped receivers
  cannot authorize virtual dispatch.
- Private construction places now have abstract compile-time authority/provenance
  facts, while reference_paths still excludes unfinished parents from GC roots.
  A forged readonly private alias passed to an inferred-reader getter could
  otherwise lose readonly child provenance in independent IR analysis. The new
  negative requires a readonly rejection; a positive readonly getter remains usable.
  Constructor preparation retains its distinct mutable-storage role; ordinary
  nonmutating helper effects remain independently checked.
- Typed descriptor errors use protected failure outputs, while legacy invalid-root/
  invalid-state ABI invariant handling remains fatal as before. Descriptor OOM,
  conflicts, exact interning, invalid ancestry and failed-batch rollback are tested.
- Contextual super recognition preserves existing variable/module bindings,
  including a qualified imported module named super; no global reserved-word policy
  was silently imposed.

The four new inheritance suites bring inventory from 47 to 51. They cover source
positives/negatives, forged IR, padding/empty classes, nested reference values,
readonly/copy semantics, exact signatures, constructor order and failure recovery,
selected receiver snapshots across allocating arguments, imports, LSP navigation,
restart contracts and old active calls retaining their virtual graph during reload.
An imported factory separates the caller from the dynamic override's owner module.

GC tests retain a derived object and its derived-only child through sole Base
carriers after factory return, with no masking host roots. Budget-one and forced
collections exercise cycles, loops, argument/return homes and failure cleanup.
Allocating JIT generations retire while retained object descriptors remain usable.
The native harness links two independently emitted generations using the shared
lowerer's symbol suffix, then calls the NEW override on an OLD object. It removes
the host handle after the new Base parameter roots register, forces deterministic
collection overlap, and requires 111 (old override: 110; base body: 100). This is a
private same-build test ABI, not a public managed-reference/aggregate C ABI.

Focused execution returned 108 in normal/O2 JIT for main, 92 for initialization,
123602 for argument/base/body order, and 113 for selected receiver preservation.
The focused nine-suite Debug pass included frontend, managed_initialization_source,
reference_values_source, composition_source, inheritance_source, both inheritance
AOT modes, runtime_generated and native lsp_protocol: 9/9, 12.34 seconds.

Exploratory builds preceded final acceptance. One early CLI attempt lacked newly
added executables while CMake/test inputs were still being edited. One exploratory
official-sanitizer link lacked the subsequently added second-generation objects;
a fresh configure included them. These were development sequencing issues, not
accepted verification. Preliminary/intermediate/candidate logs are retained under
build/phase20-verification. Final acceptance below is tied to frozen source hashes;
late changed inputs were explicitly rebuilt rather than relying on stale objects.

The first 1 MiB Debug repetition exposed a genuine semantic-stack regression in
both SDKs: frontend passed earlier groups and crashed in receiver methods. A gdb
backtrace showed repeated expression → expression_impl → method_expression frames
while checking the existing 400-call resource-limit case. Phase 20's contextual
upcast rewrite reserved an Expr temporary in every guarded recursive frame, and
parent-only handling enlarged ordinary method frames. Moved contextual rewrites
and parent-only handling into focused helpers, keeping the 256-depth guard and
all existing recovery assertions unchanged. No limit, test or stack requirement
was relaxed. The failed logs/backtrace remain in build/phase20-verification; the
acceptance matrix and stack repetitions below were rerun after this correction.

### Final acceptance evidence

Acceptance was rerun after the semantic-stack correction against 260 frozen
implementation/test/editor inputs (SHA-256 inventory in
build/phase20-verification/frozen-source.json). All hashes remained unchanged
through the final matrix, stress, stack, editor and measurement checks. All six
inventories contain the original 47 suites plus four inheritance suites: 51.

Both SDKs report LLVM 19.1.7 and contain LLJIT.h, libLLVMOrcJIT.a and
LLVMConfig.cmake. Installed SDK: /usr/lib/llvm-19, RTTI YES; complete official
archive: .toolchains/ci-investigation/LLVM-19.1.7-Linux-X64, RTTI NO. Configured
Clang/LLD 19.1.7 and existing target-wide RTTI matching are preserved. Debug and
Release use their named build types; sanitizer trees use RelWithDebInfo with
VOID_SANITIZERS=ON. No sanitizer exclusions or LLVM optimization changes were
introduced. Vptr instrumentation remains dependent on the SDK's RTTI ABI as
already established in Phase 5.

Final configure/build/CTest commands for each configured tree:

```sh
cmake -S . -B build/<tree>
cmake --build build/<tree> -j2
ASAN_OPTIONS=detect_leaks=1:halt_on_error=1:exitcode=87 \
UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1:exitcode=86 \
ctest --test-dir build/<tree> --output-on-failure -j4
```

The existing CMake caches retain the exact SDK/compiler/linker paths; fresh-build
configuration follows the documented Linux SDK instructions. Two independent
trees ran concurrently, never overlapping CTest executions within a tree. Strict
LeakSanitizer runs used untraced execution because ptrace/sandbox tracing prevents
LSan from functioning; leak detection remained enabled.

| Configuration | Final CTest | Test elapsed |
| --- | --- | --- |
| ci-debug | 51/51 | 67.68 s |
| ci-release | 51/51 | 21.96 s |
| ci-sanitizers | 51/51 | 88.94 s |
| ci-official-debug | 51/51 | 60.45 s |
| ci-official-release | 51/51 | 21.64 s |
| ci-official-san | 51/51 | 82.94 s |

This is 306 passing final suite executions. Every configure/build also returned
zero. Normal/O2 ORC, separately linked AOT, precise GC, callbacks, timeout/no-sweep,
failure cleanup, reload, diagnostic/tooling and all prior architectural assertions
remain in those suites. The inheritance CLI exercises nine source programs across
three inputs, normal/O2 and JIT/native AOT: 104 successful comparisons and four
expected constructor failures per configuration. Failures require exit 1, division
by zero and both base/derived initializer frames. Virtual result coverage includes
bool, f64, nullable references and reference-containing aggregate returns.

Targeted stress repeated eleven suites three times per configuration:
runtime_generated, runtime_callbacks, runtime_invariants, runtime_failures,
managed_source, managed_initialization_source, reference_values_source,
composition_source, inheritance_source and both inheritance AOT suites. Command:
`ctest --test-dir build/<tree> --output-on-failure -j2 -R '<anchored eleven-suite expression>' --repeat until-fail:3`.
The exact expression/commands are in build/phase20-verification/stress/results.json.
Strict sanitizer/leak settings were identical to the full matrix.

| Configuration | Stress executions | Elapsed |
| --- | --- | --- |
| ci-debug | 33/33 | 30.53 s |
| ci-release | 33/33 | 19.87 s |
| ci-sanitizers | 33/33 | 35.98 s |
| ci-official-debug | 33/33 | 28.43 s |
| ci-official-release | 33/33 | 16.72 s |
| ci-official-san | 33/33 | 31.46 s |

All 198 repetitions passed, including deterministic rendezvous, generation/root
handoffs, failures/recovery and sole-carrier collection proofs. Tests keep their
existing strict overlap/timeout assertions; no sleeps or larger workloads replace
synchronization.

Both Debug SDKs ran frontend, managed_initialization_source,
reference_values_source, composition_source and inheritance_source three times
under a 1,048,576-byte soft/hard stack limit, with CTest -j2. Installed: 15/15,
43.94 s; official: 15/15, 37.58 s. This is 30 passing executions, preserving the
existing large SCC, expanded layouts, combined depth and 400-method-chain diagnostic
recovery checks. The focused corrected 1 MiB frontend/inheritance run also passed
2/2 in 9.28 s before final matrix acceptance.

Editor verification used Node 24.19.0/npm 11.6.2:

- `npm test`: typecheck/build and unit test entry passed; a direct run of
  out/test/config.test.js confirms all four individual tests passed, including
  contextual keyword colors, indentation and existing Windows path assertions.
- `npm run package`: passed; dist/void-language-0.1.0.vsix contains 8 files,
  145.51 KB. The existing large-bundle advisory remains, without a packaging error.
- `VOIDLS_PATH=<repo>/build/ci-debug/voidls VSCODE_EXECUTABLE_PATH=/usr/share/code/code npm run test:host`:
  real VS Code 1.141.0 passed against the corrected native server. Normal,
  restricted-trust and multi-root scenarios passed; unsaved diagnostics/repair,
  navigation/stale-query checks, Unicode paths, restart/configuration/deactivation,
  two real child-process kills and strict notification counts remain exercised.
  Actual GUI execution required access outside the GUI sandbox. Native LSP protocol
  and positions tests also passed in every CTest configuration. VSCodium was not
  installed and was not claimed as verified.

Focused preliminary dispatch measurement ran only after other validation work
finished, on Linux x86_64 / AMD Ryzen 7 1700X, using both Release builds:
`build/<release-tree>/void_inheritance_tests --measure <repo>`.
The same source, storage, loop count and result are used for direct and virtual
branches. Each branch executes 100,000 calls, with three warmups and seven timed
samples; all invocations require result 1,700,000. Values below are median elapsed
microseconds for the whole invocation (including required runtime entry/poll work),
not isolated instruction latency or a production benchmark.

| SDK build | Lowering | Direct median µs | Virtual median µs |
| --- | --- | --- | --- |
| ci-release | normal | 11031 | 16143.9 |
| ci-release | O2 | 10763.9 | 16175.8 |
| ci-official-release | normal | 12566.5 | 17732.9 |
| ci-official-release | O2 | 13117.5 | 17274.7 |

Guarded virtual dispatch incurs the expected exact-type query/selection overhead
in this fixture. No timing threshold, production performance claim or correctness
relaxation was introduced. JSON/log samples remain in build/phase20-verification.

Final native demonstration: main(10)=108, initialization(10)=92,
order(10)=123602, receiver_order(10)=113, imported factory(10)=13 and polymorphic
results(10)=28 through real source in normal/O2 JIT and native linked AOT. The native
cross-generation harness requires new-code dispatch on an old object to return 111,
with only the new Base parameter roots retaining the old derived object and child.

`git diff --check` passed; the seven-runner workflow and LLVM/RTTI/sanitizer policies
are unchanged. The shared generated-executable family still compiles native_link.cpp
as a direct input under the configured Windows CRT policy. No commits, pushes,
branches or remote modifications were made.

Phase 20 is ready for owner review and the seven hosted jobs, not yet hosted-verified.
Windows Debug/Release and macOS ARM64 Debug/Release remain unverified locally; the
hosted Ubuntu Debug/Release/Sanitizers jobs must also validate the pushed revision.
Linux results do not prove those target layouts/toolchains. The experimental runtime
is not presented as production-ready. Interfaces, source downcasts/type patterns,
protected access, hiding, covariance, abstract/final, open-world loading, implicit
constructor chaining and external managed ABI remain outside this phase. After
hosted acceptance, structural interfaces are a separate design/approval task.


## Phase 20 Windows AST relocation correction — run #34 (13 October 2026)

Investigated the actual completed Windows job logs for run 38514742460,
commit d8f7a2e6cc577f2c926525b229d0325dcce87330. Windows Debug job
115690896295 and Release job 115690896246 both fail compiling compiler.cpp,
before linking or test execution. Retrieved individual job logs through the
GitHub read-only job-log API while the other five jobs were still running;
the overall run was not yet complete. Logs are retained locally under
build/phase20-windows-move-verification/windows-{debug,release}.log.

Both logs use the official Clang-cl 19.1.7 toolchain with MSVC STL
14.44.35207. The instantiation chain is Structure-vector reallocation →
uninitialized COPY of Structure → copy of vector<Statement> → construct_at
with Statement& → implicitly deleted Statement copy constructor (unique_ptr<Expr>).
The failing push_back(std::move(structure)) is at hosted compiler.cpp:569.
It moves the new argument; the invalid copy is relocation of existing elements.

Structure's implicit copy trait was misleading: vector<Statement>'s copy constructor
is declared even though its body cannot copy Statement. Phase 20 adds
optional<Type> base_type; Type owns a std::set of readonly reference paths.
MSVC tree moves allocate a sentinel and can throw, so this member prevents
Structure's implicit move from being noexcept. Vector therefore selects the
apparently available copy. Linux libstdc++/libc++ instead report a nonthrowing
Structure move and never instantiate that latent invalid copy. Microsoft's
[VS 2022 STL tree implementation](https://github.com/microsoft/STL/blob/vs-2022-17.14/stl/inc/xtree)
and [vector reallocation selection](https://github.com/microsoft/STL/blob/vs-2022-17.14/stl/inc/vector)
confirm the throwing tree move and move-if-nothrow-or-noncopyable policy;
the hosted logs confirm the selected copy path on the actual runner.

Small correction: explicitly delete Structure copy construction/assignment,
default its default/move construction and move assignment, with no explicit
noexcept. Member-derived exception specifications remain truthful. Statement
and Expr ownership are unchanged; no AST clone, forced noexcept, capacity/reserve
workaround, allocator change, compiler-semantic change or build-policy change.
Compile-time assertions on the actual internal types require noncopyable
Statement/Structure and movable Structure on every compiler/STL build.

Added a normal/O2 source regression to the existing inheritance_source suite:
32 value payloads, 32 managed roots and 32 derived structs force repeated
Structure-vector growth with engaged base Types and unique expression defaults.
All 32 constructors preserve their own nested/default values; entry(10) must
return 1530. No existing assertions or suites were removed (51 remain).
Six standalone syntax probes include the actual pre-fix/fixed compiler translation
units, using Clang 19.1.7/libstdc++, Clang 19.1.7/libc++ and GCC 16.2/libstdc++.
Each verifies the pre-fix false-positive copy/nonthrowing-move traits and corrected
noncopyable/movable contract. All six passed; these are Linux compile probes,
not a Windows/MSVC execution claim. Focused Debug tests passed 9/9 in 16.24 s:
frontend, inheritance source/CLI/both native AOT modes, managed initialization,
reference values, composition and native LSP protocol.

Final post-correction validation completed against unchanged frozen source/test
hashes (260 inputs). All six configure/build/CTest sequences returned zero:

| Configuration | CTest | Test elapsed |
| --- | --- | --- |
| ci-debug | 51/51 | 66.84 s |
| ci-release | 51/51 | 22.47 s |
| ci-sanitizers | 51/51 | 90.66 s |
| ci-official-debug | 51/51 | 62.79 s |
| ci-official-release | 51/51 | 24.46 s |
| ci-official-san | 51/51 | 78.23 s |

This is 306 passing suite executions, preserving all 51 suites and their prior
assertions, including runtime/GC stress proofs, normal/O2 ORC JIT, separately linked
AOT, constructor/failure handling, readonly, reload, diagnostics and LSP checks.
Both SDKs are complete LLVM 19.1.7 installations with headers, OrcJIT library and
LLVMConfig.cmake verified. Installed RTTI YES / official RTTI NO still match all
project targets. Sanitizer trees retain RelWithDebInfo, VOID_SANITIZERS=ON,
-fsanitize=address,undefined and frame pointers; no exclusions were added.

Commands for each existing pinned/configured tree were:

```sh
cmake -S . -B build/<tree>
cmake --build build/<tree> -j2
ASAN_OPTIONS=detect_leaks=1:halt_on_error=1:exitcode=87 \
UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1:exitcode=86 \
ctest --test-dir build/<tree> --output-on-failure -j4
```

Strict sanitizer execution used the same untraced mode needed for LeakSanitizer;
leak detection was never disabled. Commands, SDK checks and per-step logs are in
build/phase20-windows-move-verification/matrix. The six actual-AST syntax probes
and exact compiler arguments are recorded in traits-results.json. These Linux
probes include libc++ only for source syntax/traits, not a claim that the prebuilt
LLVM SDK was relinked against another standard-library ABI.

Both Debug SDKs reran frontend, managed_initialization_source,
reference_values_source, composition_source and inheritance_source under a 1 MiB
soft/hard stack limit, -j2, --repeat until-fail:3: installed 15/15 in 47.40 s,
official 15/15 in 38.94 s (30 executions). Existing bounded-recursion/recovery
assertions and the new relocation program all remain active.

Editor checks: npm test passed typecheck/build/unit entry; direct execution of
out/test/config.test.js confirmed all four individual tests. npm run package
passed, producing the unchanged 8-file / 145.51 KB local VSIX. Real VS Code 1.141.0
with the corrected Debug voidls passed normal, restricted-trust and multi-root
integration scenarios, including actual native process kills, exactly two expected
termination notifications, restart/deactivation and stale-query protections.
Native LSP coverage passed in all six matrices. No extension code or grammar was
changed. VSCodium remains unavailable locally and is not claimed verified.

Final git diff --check passed. Only compiler.cpp, the inheritance regression and
WORK_LOG.md changed. Runtime/backend/CMake/workflow and the Windows CRT correction
are untouched. No commits, pushes, branches or remote modifications were made.
Windows Debug/Release remain unverified until hosted CI compiles and executes this
corrected revision. Local Linux tests and STL-source analysis establish the cause
and portability correction, not Windows success. Ready for owner review and the
next seven-job hosted gate.


## Phase 21 investigation — structural interfaces (design only)

Owner reports Phase 20 LOCKED at GitHub Actions run #35: all seven jobs passed
51/51 suites. This entry records local investigation, not independent retrieval
of those hosted logs or authorization to implement Phase 21.

Read the canonical Bible's interface/object-model, conversion/generic/readonly
boundaries, Strategy, decisions/issues and Phase 16–20 implementation contracts.
Inspected the actual declaration/resolver/member selector, inheritance and staged
construction checks, nominal Type/FieldType arena and independent IR validation,
reference-origin/effect fixed point, LLVM receiver/virtual dispatch/root lowering,
Runtime descriptor admission/query/publication and shared analysis/tooling/reload
records. Interfaces are not implemented: current reference checks explicitly use
managed_ref; IR lacks requirement/witness/prepared-erased-receiver facts; runtime
nominal records contain layout/identity but no method/code binding.

Produced INTERFACES_PROPOSAL.md with IF1–IF8 recommendations awaiting approval.
Recommend managed-backed views first, capturing complete owner identity plus an
immutable runtime-owned data witness for the converting static view and selected
accessible method routes. Existing generation-owned guarded dispatch plans consume
that binding and reuse direct/virtual calls. Witnesses contain no code addresses,
generation-local indexes, capability tags or code leases; metadata extends the
existing Runtime admission snapshot, not a second heap/runtime/root system.
Standalone value erasure/boxing remains deferred. Managed owners can satisfy
requirements through embedded value methods borrowing original storage. Interface
views may be reference leaves in existing copied values, with unchanged RV/P3 rules.

Two concrete Phase 20 constraints were reproduced rather than assumed:

- Base promoting meter.read and Derived declaring a real read give different
  results on the same complete object: 7 versus 20 (combined 720). Dynamic-type-only
  interface binding would lose static-view lookup semantics.
- A promoted managed child call whose argument replaces that child selects the
  old child before arguments (combined 1190, not late-projection 9190). Interface
  dispatch cannot defer component projection until after argument evaluation.

Temporary .void reproducers and command/result JSON are in
build/phase21-investigation. Both programs ran through each unchanged Debug SDK's
voidc in normal/O2 JIT: eight executions, all expected results. They prove existing
lookup/evaluation contracts, not Phase 21 interface code generation or AOT support.

Focused unchanged CTest baseline on each installed/official LLVM 19.1.7 Debug tree:

```sh
ctest --test-dir build/<debug-tree> --output-on-failure -j2 \
  -R '^(inheritance_source|composition_source|reference_values_source|managed_initialization_source|lsp_protocol)$'
```

Installed build/ci-debug: 5/5 in 7.99 s. Official build/ci-official-debug: 5/5 in
7.53 s. Logs are installed-baseline.log and official-baseline.log under the same
investigation directory. No full matrix, sanitizer/stack/editor rerun was needed
for documentation-only work; those remain implementation acceptance gates along
with all 51 suites, GC stress, normal/O2 JIT, real linked AOT and seven hosted jobs.

Approval questions include exact requirements/access and factory exposure,
contextual conversions, static-view witness/equality policy, deferred standalone
value representation, conservative project-wide per-requirement effect precision,
pre-argument receiver preparation and metadata/reload policy. Signature matching
must not guess among ambiguous promoted candidates. Shared IR needs minimal logical
using/access/import facts so independent validation can reject forged conformance;
the runtime must not grow a duplicate source type checker. Required interface leaves
must integrate C1/S1/P4 readiness/publication, rather than erasing unfinished self.

No compiler, runtime, editor, tests, build or workflow changes. No commits, pushes,
branches or remote modifications. Stop for owner review of IF1–IF8; only the proposal
and this work-log entry are changed. Language Bible/locked phase decisions are not
amended by recommendations awaiting approval.

## 14 October 2026 — Phase 21 structural interfaces, approved implementation

The owner approved IF1–IF8 in INTERFACES_PROPOSAL.md. Phase 20 remains locked at
owner-reported hosted run #35, seven jobs with 51/51 suites. Work is local only:
no commits, pushes, branches, remote changes or workflow changes.

Implemented managed-backed structural views through the existing pipeline:

- Bodyless nominal interface requirements; exact ordered signatures/const formals,
  original ownership/imports and conversion-scope access, including private factories.
- One shared real/inherited/one-hop member selector for source and IR binding. Direct
  precedence and ambiguity are decided before signatures. Standalone value erasure,
  interface width/downcasts, boxing and broader syntax are explicitly deferred.
- Identity-preserving concrete-to-interface conversions, nullable canonical pairs,
  locals/fields/value carriers/arguments/results, same-interface identity equality,
  source static Base/Derived selection, and ordinary/virtual/promoted calls.
- Per-requirement effect/provenance fixed points over every eligible provider and
  virtual family, preserving receiver versus explicit argument origins. P3/RV
  capabilities survive copies/casts/returns; scalar value storage remains independent.
  Empty families are conservatively unknown. C1/S1/P4 are retained for interface
  defaults, required fields, complete children and unfinished-self rejection.
- Independent IR validates complete nominal/binding universes, scopes, signatures,
  routes, access, capabilities and preparation identity/definite initialization.
  Prepared tokens cannot escape through fields, formals or results. Omitting an
  eligible nominal/provider cannot certify an optimistic readonly call.
- Shared LLVM pair layout and precise object-word offsets, including nested values;
  original receivers are prepared before argument evaluation/control flow. Private
  tokens root the complete owner and selected managed component, excluding metadata
  and interior addresses. Existing module-owned DataLayout caching remains intact.
- Atomic NativeSafe Runtime metadata admission now includes immutable witnesses in
  the existing snapshot. OOM/conflicts roll back; opaque membership/domain checks
  precede use. Witnesses own data only, with no code/AST/LLVM pointers, leases,
  generation indexes or runtime permission flags. Dispatch uses generation plans and
  existing ordinary/virtual functions/whole-call leases. JIT and AOT share lowering.
- Source/tooling records interface/requirement categories and provider dependencies.
  Constructor closure freezes every eligible interface/virtual provider. Compatible
  provider-body reload remains possible. The extension uses existing keyword colors
  and adds interface declaration indentation without lifecycle changes.

New source, normal/optimized native AOT and CLI suites bring the total to 55,
preserving all 51 existing suites and assertions. CMake reuses the managed-source
family and its direct native CRT-selection input; no Windows linker workaround.
Actual `.void` fixtures cover promotion, nullable copies, private factories, static
binding/identity, receiver replacement, cycles, nested handoffs and allocation stress.
The native harness links two independent generations and passes an old typed holder
pointer; it does not assume a two-pointer C++ interface calling ABI.

Focused verification established:

- The six unchanged baseline focus suites passed 6/6 (13.72 seconds) after initial
  declaration/type foundations. This was baseline preservation, not interface proof.
- New interface source and both native AOT suites passed 3/3 during focused work.
  Later expanded source checks passed, including actual active-call/new-body leases,
  metadata conflict/allocation rollback, deterministic collection overlap, timeout
  no-sweep/recovery, failure cleanup, retirement and stable witness reuse.
- Nested handoff probes find the intended object only in aggregate roots after its
  concrete factory returns; unrelated loop garbage is not counted as its root.
  Selected-component probes use a separate owner factory, replace its child during
  arguments, allocate again, and require the original child solely in the prepared
  snapshot's second root word. No host Handle masks either proof. Both normal/O2
  JIT and linked native AOT exercise these cases.
- Native LSP protocol coverage for imported interface declaration/requirement
  navigation, unsaved signature mismatch, readonly provider-effect changes,
  stale-query rejection and repaired revisions passed (0.39 seconds).
- Source resource tests exercise both hard analysis ceilings: 65536 scope/view/
  interface combinations and 8388608 member-inventory visits. Neither limit truncates
  candidates. Existing layout/expansion/S1 limits and small-stack guards are retained.

The first full matrix pass found a diagnostic wording regression introduced here:
managed null conversion used a generic message. Strict managed_cli and
reference_values_cli assertions caught it. Restored the exact existing `Null managed
reference in checked conversion` wording, with a separate interface message. No
existing assertion changed. An initial new template fixture also shadowed receiver
`c`; renamed the module constants to respect the existing no-shadowing rule. These
first-pass logs remain under build/phase21-verification/first-matrix-pass.

The corrected source is frozen for the final six-configuration matrix. Final matrix,
stress, 1 MiB stack, editor/package results and outstanding hosted status follow
below; earlier intermediate passes are not represented as final acceptance.

### Verification corrections: stale build objects and small-stack hint traversal

An incremental installed-SDK sanitizer run failed the new null-interface diagnostic
assertion, without an ASan/UBSan report. The sanitized CLI reproduced the correct
checked error in normal/O2 modes. `strings` on the test object still showed the old
`Null reference` comparison; the source already compared `Null interface reference`.
Its timestamp was newer than the edited source, so CMake had correctly considered
that stale object current. Clean builds of all six configurations then passed
55/55 with strict leak detection unchanged. Retain the incremental failure logs;
they are not accepted as final evidence. Final dependency timestamps are refreshed
and frozen-input hashes checked to prevent stale-object claims.

The 1 MiB repeated test gate subsequently found the existing 400-call receiver-chain
regression crashing in frontend on both SDKs. A separate `voidc check` reproducer
and GDB backtrace under exactly 1048576 bytes identified repeated
`Semantics::anchor` frames at compiler.cpp:1084 before the semantic expression
nesting guard. Phase 21 increased type/frame size and exposed that unbounded hint
traversal. Added the same existing 256-depth semantic budget to hints; exhaustion
falls back to ordinary checking, which retains the original source nesting error.
No stack limit, expression limit, assertion or test changed. The reproducer now
reports VOID1107 and the independent later VOID2003 rather than crashing. The
unchanged full frontend CTest passes under 1 MiB (7.95 seconds). Final six-configuration
and repeated small-stack/stress results below include this correction.

### Final Phase 21 acceptance evidence

The final frozen 275 source/test/editor/build inputs match their SHA-256 snapshot.
Both complete SDKs report LLVM 19.1.7 and contain ORC headers, libraries and
LLVMConfig.cmake. Installed SDK RTTI is YES; official archive RTTI is NO. Existing
per-SDK RTTI correction and all strict sanitizer settings remain intact.

| SDK/configuration | Configure | Build | Full CTest | Test seconds |
| --- | --- | --- | --- | --- |
| ci-debug | pass | pass | 55/55 | 66.34 |
| ci-release | pass | pass | 55/55 | 25.63 |
| ci-sanitizers | pass | pass | 55/55 | 83.40 |
| ci-official-debug | pass | pass | 55/55 | 62.80 |
| ci-official-release | pass | pass | 55/55 | 23.06 |
| ci-official-san | pass | pass | 55/55 | 83.95 |

Commands: `cmake -S . -B build/<tree>`, `cmake --build build/<tree> -j2`,
`ctest --test-dir build/<tree> --output-on-failure -j4`. Full clean builds first
established every configuration; the final three-line hint-budget correction was
rebuilt in all six after refreshing its dependency timestamp. The table is the
final post-correction result, not an earlier intermediate pass.

Strict runs used `ASAN_OPTIONS=detect_leaks=1:halt_on_error=1:exitcode=87` and
`UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1:exitcode=86`. They ran outside the
sandbox to permit LeakSanitizer; no sanitizer or assertion was disabled.

Repeated stress used CTest `--repeat until-fail:3 -j2` over 14 suites: generated
runtime, callbacks, invariants, failures, managed/managed-init/reference-value/
composition/inheritance source, both inheritance AOT and interface source/AOT.
All six configurations passed: 252 repeated suite executions.

| Stress tree | Status | Seconds |
| --- | --- | --- |
| ci-debug | 14/14, three repeats | 40.30 |
| ci-release | 14/14, three repeats | 22.93 |
| ci-sanitizers | 14/14, three repeats | 47.41 |
| ci-official-debug | 14/14, three repeats | 38.64 |
| ci-official-release | 14/14, three repeats | 20.66 |
| ci-official-san | 14/14, three repeats | 45.35 |

`RLIMIT_STACK` soft/hard limits were exactly 1048576 bytes. Frontend, managed-init,
reference-value, composition, inheritance and interface source suites passed three
times on each Debug SDK: 36 executions, ci-debug 53.93s, ci-official-debug 55.53s.
Existing stack/nesting assertions remain intact.

Extension: `npm test`, explicit `node --test --test-isolation=none` (4/4 unit cases),
`npm run package` (149020-byte local VSIX), and real VS Code 1.141.0 integration
passed. Native-process kill notifications, strict lifecycle counts, unsaved repair/
navigation, close/reopen, restart/configuration/failure/deactivation, trust and
multi-root assertions remain. Final host integration uses the post-stack-fix native
Debug voidls. VSCodium is not installed locally; do not claim a separate VSCodium
host execution. Node remains 24.19.0; no package dependency versions changed.

Actual native demonstration: JIT main(10) = 28; static-view/identity JIT = 720;
separately linked selection executables in normal and optimized modes = 1200.
The CLI suite checks all eight real interface fixtures with -1/0/10 through normal/
O2 JIT and linked AOT. Interface-bearing value argument/return handoffs and old
interface fields/new-code dispatch also execute in the independent native harness.

Artifacts/logs: `build/phase21-verification/{matrix,stress,stack}/`, SDK verification,
frozen-source.json, focused source/AOT/CLI evidence, small-stack backtrace/reproducer
and editor unit/package/host logs. Initial diagnostic/stale-object failures and the
clean pre-stack-fix matrix are retained separately.

No owner-level contract contradiction was needed. Deferred behavior is documented
in INTERFACES.md and KNOWN_ISSUES.md; no boxing, width/downcasts, interface inheritance,
generic/Self/default bodies, open-world providers, new entry policy, allocator/root
registry or runtime permission system was added. Phase 21 is ready for owner review
and the unchanged seven-job hosted CI gate. Windows/macOS and the hosted Linux SDK/
runner matrix remain unverified until the owner pushes and runs CI. Linux success
is not reported as Windows/macOS verification. No commits, pushes, branches or
remote modifications were made.


## 14 October 2026 — `pass` statement and editor highlighting

Baseline: owner reports Phase 21 locked after GitHub Actions run #36 passed all
seven hosted jobs. This change preserves the 55-suite matrix and the seven-job
workflow; hosted results below do not claim verification of these new changes.

Investigation checked indentation lexing, parser block/recovery and bodyless
interface requirement parsing, statement flow analysis, construction readiness,
reference capabilities, shared IR lowering and source/native test drivers. Ordinary
blocks already require a statement. Interface requirements have their own signature
parser and never use ordinary statement bodies.

Implementation adds the reserved `pass` keyword, one no-op AST statement kind,
normal semantic fallthrough and an empty lowering case. No IR opcode, backend
operation or runtime machinery was added. Required return paths and required
managed-field initialization remain enforced. `pass` cannot be an expression,
identifier, call, field declaration or concrete interface requirement body.

`examples/pass.void` covers no-result functions, value initialization and methods,
nested conditionals, conditional/unbounded loops, ordinary execution after `pass`,
and preserved break/continue behavior. Inputs -1/0/1/10 produce 7/8/9/18. Existing
frontend/source_cli suites execute it with normal/O2 JIT and independently linked
native AOT, verifying emitted debug information and preserving native CLI input
assertions. Frontend coverage also executes a managed initializer and a bodyless
interface fulfilled by a real no-op method, and rejects missing returns, empty
comment-only bodies, malformed/expression uses, unavailable required references and
interface bodies. Recovery verifies two accurate independent diagnostics after a
malformed `pass` statement. The existing interface fixture adds `pass` before and
after a promoted mutation, retaining all expected results in JIT and linked AOT.

Three old fixture helpers named `pass` were renamed to `forward` in nullable
interface and inheritance tests/examples because the new statement keyword is
reserved. Their behavior and assertions are unchanged. No existing assertion or
suite was removed.

VS Code/VSCodium grammar places `pass` in the exact `keyword.control.void` rule used
by `return`. The presentation test asserts both share that rule and rejects partial
matches (`passing`, `bypass`). Canonical LANGUAGE_BIBLE.md Section 27.2 documents
purpose, syntax, legal contexts, control flow, supported loop examples, invalid
empty/missing-return examples and bodyless interfaces; Section 65 records approval
and Section 66 replaces the obsolete unresolved placeholder decision. Range loops
remain deferred. SOURCE_COMPILER.md describes the AST/IR boundary.

Focused verification: `cmake --build build/ci-debug -j2`, then
`ctest --test-dir build/ci-debug --output-on-failure -j2 -R
'^(frontend|source_cli|interfaces_source|interfaces_aot_normal|interfaces_aot_optimized|interfaces_cli|inheritance_source)$'`
passed 7/7 in 15.07 s. Extension `npm test` passed; the explicit Node unit run
(`node --test --test-isolation=none out/test/config.test.js`) confirmed 4/4 cases.
`npm run package` produced `editors/vscode/dist/void-language-0.1.0.vsix`.
Real editor integration (`VOIDLS_PATH=<repo>/build/ci-debug/voidls
VSCODE_EXECUTABLE_PATH=/usr/share/code/code npm run test:host`) passed normal,
restricted-trust and multi-root scenarios, including actual native process kills,
notifications, restart and clean deactivation. VS Code was executed on Linux;
VSCodium is not installed and is not claimed as a separate verified editor host.

Full verification completed with `python3 build/pass-verification/validate.py`:
each existing tree was configured (`cmake -S . -B build/<tree>`), built
(`cmake --build build/<tree> -j2`) and tested (`ctest --test-dir build/<tree>
--output-on-failure -j4`). All 55 suites passed in all six configurations (330/330
suite executions), including the existing architectural, GC/coordinator/callback,
reload, independent IR, native AOT, source diagnostics and LSP regressions.
Debug build timings are incremental after the focused build; compilation inputs
were unchanged throughout the complete matrix.

| Build tree | Configure (s) | Build (s) | CTest (s) | Result |
|---|---:|---:|---:|---|
| ci-debug | 8.02 | 1.86 | 68.03 | 55/55 |
| ci-release | 7.95 | 53.36 | 27.21 | 55/55 |
| ci-sanitizers | 10.84 | 125.30 | 101.08 | 55/55 |
| ci-official-debug | 8.26 | 34.91 | 64.79 | 55/55 |
| ci-official-release | 9.39 | 69.79 | 29.01 | 55/55 |
| ci-official-san | 8.68 | 118.56 | 82.09 | 55/55 |

Both complete SDKs were verified as LLVM 19.1.7, with LLJIT headers,
libLLVMOrcJIT.a and LLVMConfig.cmake present: `/usr/lib/llvm-19` (RTTI YES) and
`.toolchains/ci-investigation/LLVM-19.1.7-Linux-X64` (official archive, RTTI NO).
The existing matched toolchain/RTTI correction was preserved. Debug and Release
used their respective build types; sanitizer trees used RelWithDebInfo with
VOID_SANITIZERS=ON. Strict options were
`ASAN_OPTIONS=detect_leaks=1:halt_on_error=1:exitcode=87` and
`UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1:exitcode=86`.
The matrix ran outside the process-inspection sandbox so LeakSanitizer could work;
leak detection and sanitizer assertions were not disabled or suppressed.

`python3 build/pass-verification/stack.py` repeated the six existing focused suites
(frontend, managed_initialization_source, reference_values_source,
composition_source, inheritance_source, interfaces_source) three times per SDK
Debug tree with RLIMIT_STACK=1,048,576 bytes. All 36 executions passed: installed
SDK 64.06 s, official SDK 57.89 s. A separate native demonstration recorded all
16 normal/O2 JIT and linked AOT invocations of `examples/pass.void`, confirming
entry(-1/0/1/10) = 7/8/9/18.

Extension verification used Node 24.19.0 and npm 11.6.2. The VSIX contains the
updated grammar (149,023 bytes packaged). Logs, executed commands, SDK verification,
full matrix results, small-stack results, source hashes (276 inputs) and native
outputs are retained under `build/pass-verification/`. `git diff --check` passed.
The pre-existing owner changes in docs/WRENCH_CLANKER.md were left untouched.

Local implementation and validation are complete. The owner must still run the
unchanged seven-job hosted gate to verify these changes on Windows/macOS and the
hosted Ubuntu environment. No Windows/macOS execution is claimed locally. There
are no new runtime/IR limitations; range/collection iteration remains deferred.
No compiler/runtime feature beyond `pass`, no workflow/build changes, and no
commits, pushes, branches or remote modifications were made.

## 14 October 2026: Phase 22 UTF-8 strings, design and investigation only

The owner reports phases 1 through 21 complete and hosted-verified: Phase 21
Actions run #36 and the subsequent pass improvement run #37 each passed seven
jobs. This is owner-reported evidence; no hosted run was inspected here. Initial
read-only git status was clean. Workspace and pasted assignment were accessible.
No AGENTS.md was found under the project parent. No implementation was authorized.

Reviewed current LANGUAGE_BIBLE, DEVELOPMENT_STRATEGY, DECISIONS, KNOWN_ISSUES,
WORK_LOG, SOURCE_COMPILER, GC_RUNTIME and INTERFACES, with relevant initialization,
reference-value and analysis compatibility contracts. Inspected actual source
validation/lexing/parsing/typed AST, pure constants/default construction, shared
Type/reference shapes, independent IR validation, LLVM layouts/root homes/calls/
returns/failure paths, Heap and coordinated complete publication, private runtime
ABI/bootstrap, generation leases/contracts, native_utf8, LSP positions/snapshots,
editor grammar, relevant tests and the seven-job workflow.

Created STRINGS_PROPOSAL.md. Principal findings:

- Valid UTF-8 source and immutable byte snapshots exist. Double quotes currently
  produce import-path tokens without escapes, not string expressions. Builtin
  string types, byte constants, string operations and ordinary string coloring
  are absent. The current native_utf8 helper validates and owns a terminated
  temporary while rejecting NUL; it is not a general VOID string implementation.
- Heap TypeLayout supports variable positive sizes and leaf payloads, but admitted
  nominal descriptors require exact fixed sizes. A variable tail needs explicit
  intrinsic payload classification/extent validation, not a fixed descriptor
  workaround, per-length metadata accumulation or a second memory system.
- reference_paths serves both tracing and readonly/effect analysis. Immutable
  strings need traced leaves without mutable-referent requirements, while mixed
  managed/interface capabilities retain their current restrictions. Simply adding
  string to reference_value would also impose incorrect nominal/default/null rules.
- Existing root frames, nested target offsets, argument/result homes, no-GC
  handoffs, selected-owner snapshots, staged fields and complete publication are
  reusable. Current coordinated-module inference scans nominal declarations;
  a string-only program needs explicit coverage. Source strings still use the
  private call convention and current i32 CLI entry adapter, not a public C ABI.
- Pure constants currently contain bits/recursive fields. Nonempty immutable text
  templates and fallible runtime materialization are a proposed extension to
  current pure-default rules, requiring approval. C1/S1/P3/P4/RV and independent
  IR validation must remain authoritative.
- Persistent bytes cannot borrow source/LLVM/JIT globals. Runtime-owned data and
  whole-generation leases already separate heap and executable lifetime. The
  same separation must hold for old strings used by a new generation.

Recommended approval items S1-S8 cover flat immutable intrinsic leaves in the
existing heap, internal zero-as-empty, existing publication/root/failure machinery,
strict UTF-8/escapes, exact-byte equality/no normalization, reserved automatic
interpolation syntax, deferred indexing/character/length APIs, +/+= and pure byte
templates, embedded NUL independent of terminated native APIs, copying boundaries,
JIT/AOT/reload compatibility and a coherent first slice. The proposal presents
alternatives, proposed compiler byte limits, valid/invalid future examples,
implementation order, targeted regressions, risks and full eventual acceptance.
These are recommendations, not owner-approved semantics.

Document discrepancies were retained and explained: original Bible/Strategy opening
status is historical; current code/DECISIONS settle C++20 and LLVM 19.1.7.
GC_RUNTIME's source/budget exclusions are historical. KNOWN_ISSUES/INTERFACES
pending Phase 21 hosted wording predates the owner's new report. No Language Bible
change or unrelated status cleanup was made.

### Focused local verification actually executed

- `ctest --test-dir build/ci-debug -N`: 55 registered suites.
- `/usr/lib/llvm-19/bin/llvm-config --version`: 19.1.7. Existing Debug cache uses
  `/usr/lib/llvm-19/lib/cmake/llvm`, CMAKE_BUILD_TYPE=Debug, sanitizers OFF.
- `cmake --build build/ci-debug -j2`: successful incremental build. A second
  invocation completed successfully with its full log captured; no compiler,
  runtime, test, build or editor source was modified.
- `ctest --test-dir build/ci-debug --output-on-failure -j2 -R
  '^(frontend|native|gc|reload|runtime_generated|runtime_failures|reference_values_source|managed_initialization_source|interfaces_aot_normal|interfaces_aot_optimized|lsp_positions|lsp_protocol|source_cli)$'`:
  **13/13 passed in 13.63 seconds**. Existing tests cover normal/O2 JIT,
  independently linked AOT, native malformed/NUL validation, rooting/failures,
  aggregate construction and LSP positions/protocol. These are unchanged baseline
  tests, not proof of strings. Full six-configuration matrix, sanitizers, second
  SDK execution, small-stack, editor host/VSIX and hosted CI were not repeated for
  this documentation-only assignment.
- Temporary CLI `voidc check` probes: a Unicode comment checked successfully;
  a quoted Unicode expression failed with VOID1101; string parameter/result
  annotations failed with VOID1103; malformed comment bytes C0 AF failed with
  VOID1001. Temporary inputs were automatically removed. The first Python probe
  harness failed to decode the malformed-source diagnostic because the CLI emits
  raw invalid bytes in its excerpt. Repeating with byte capture/backslash display
  confirmed the expected compiler rejection. This separate renderer concern is
  recorded for review and was not fixed or hidden.

Build/CTest log copies are retained under `build/phase22-investigation/`.
Final `git diff --check` and explicit whitespace/em-dash checks on both new document
sections passed. Read-only git status confirms only STRINGS_PROPOSAL.md and this
log changed. No implementation, tests, Language Bible, SDK or workflow was changed;
no Git operations beyond read-only inspection were performed. Stop for owner/
wrench-clanker review of S1-S8; implementation requires explicit authorization.

## 14 October 2026: approved Phase 22 UTF-8 diagnostic-output amendment

Boss and wrench clanker explicitly authorized fixing the recorded VOID1001
rendering defect, superseding the earlier instruction to leave it documented
without implementation. This authorization is limited to malformed-source
diagnostic output; S1-S8 string implementation remains unapproved.

Reproduced the defect using a new strict-decoding real CLI regression before
changing production code. The original compiler rejected the malformed file,
but Python's strict stderr decode failed on byte 80. The cause was direct copying
of raw rejected source into render_diagnostic's snippet. SourceFile correctly
retained the original rejected bytes and first-invalid-byte span.

Changed only that shared compile-time renderer in src/frontend/source.cpp. It
uses the existing scalar validator, retains each valid UTF-8 scalar and renders
each malformed byte as uppercase ASCII `\xHH`. It calculates caret indentation
and underline width from the displayed scalar/escape widths. Related snippets
use this same path. SourceFile validation, line indexes, scalar locations, raw
snapshots/spans, runtime excerpts and LSP position conversion are unchanged.
No buffer normalization, replacement source text, second renderer or broader
architecture change was needed.

Added meaningful regressions without changing the 55-suite inventory:

- tests/frontend/diagnostics.hpp verifies original rejected bytes and precise
  first-byte span, scalar position after multibyte/astral text, strict-valid
  rendered UTF-8, four-character escaped-byte underline, caret alignment after
  two escaped bytes and related-location rendering from the same snapshot.
- tests/integration/utf8_diagnostics.py executes 218 real CLI probes. It decodes
  both stdout/stderr strictly and checks check/run/emit rejection, exact paths,
  lines/columns, escaped snippets/carets, source preservation and protected object
  bytes. Cases include stray continuations, two/three-byte overlong sequences,
  surrogate/out-of-range scalars, forbidden leads, bad continuations, truncation,
  BOMs, all three line endings, EOF without newline and valid non-ASCII text
  around damage. Only output CRLF is normalized after strict decoding for portable
  transcript comparison; source bytes are never normalized. Canonical generic
  paths account for Windows separators and macOS temporary-directory aliases.
- The existing source_cli/diagnostics.cmake invokes this script through the
  already required configured Python interpreter. CMake passes that interpreter
  to the existing suite; no new dependency, test bypass or platform skip is added.
- tests/lsp/protocol.py verifies a real malformed disk source reports VOID1001
  at the original UTF-16 range, an unsaved valid overlay clears it, and closing
  the overlay restores the disk diagnostic. Original disk bytes remain identical.

Updated DIAGNOSTICS.md with the rendering/coordinate contract and regression
coverage. KNOWN_ISSUES.md marks the recorded defect fixed locally, with hosted
acceptance still pending. STRINGS_PROPOSAL.md replaces its unresolved-observation
wording with this approved correction and preserves the string approval gate.
Earlier log entries remain historical evidence, superseded by this amendment.

### Verification executed for the correction

`cmake --build build/ci-debug -j2` succeeded. The new strict CLI script passed
218/218 probes directly. Focused frontend/lsp_positions/lsp_protocol passed 3/3
in 7.89 s. The additional malformed-disk LSP case passed separately in 0.42 s.

Final Debug `ctest --test-dir build/ci-debug --output-on-failure -j4` passed
**55/55 in 61.67 s**, including the new CLI checks inside source_cli and all
existing source/JIT/AOT/runtime/editor-protocol acceptance. No assertion or
existing suite was removed or weakened.

Built targets `voidc void_tests voidls void_lsp_tests` with `-j2` in both
build/ci-official-debug and build/ci-sanitizers. Focused CTest command for each:
`ctest --test-dir build/<tree> --output-on-failure -j2 -R
'^(frontend|source_cli|lsp_positions|lsp_protocol)$'`.

| Configuration | Result | Seconds |
| --- | --- | ---: |
| Official LLVM 19.1.7 Debug | 4/4 | 7.65 |
| Installed LLVM 19.1.7 strict ASan/UBSan, RelWithDebInfo | 4/4 | 10.94 |

Strict sanitizer options remained
`ASAN_OPTIONS=detect_leaks=1:halt_on_error=1:exitcode=87` and
`UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1:exitcode=86`. The first sandboxed
attempt failed because LeakSanitizer cannot inspect processes under ptrace.
The approved unrestricted rerun passed all four suites with leak detection and
all sanitizer checks enabled; the failed sandbox log is retained separately.
Both focused configurations include the 218-probe CLI regression. Release, the
remaining sanitizer/SDK combinations, small-stack, real GUI editor/VSIX and hosted
Windows/macOS jobs were not rerun for this narrow renderer correction. Actual LSP
process behavior was tested, not a claim of a new GUI editor run. All seven hosted
jobs remain the platform acceptance gate.

Logs are retained under build/phase22-investigation/utf8-diagnostics/. The fix is
verified locally and is no longer recorded as an unresolved renderer defect.
String implementation, Language Bible, LLVM version, runtime memory semantics
and the hosted workflow are unchanged. No Git mutations were performed.


## Phase 22 hosted run #38: optimized frontend stack investigation (14 October 2026)

Owner requested evidence before changing code, preservation of assertions,
optimization and architecture, focused regression coverage, and a stop before any
owner-controlled push. No Git mutation or hosted run was initiated.

### Evidence and comparison

Read the actual Windows Release job 116036122476 for run 38617462093, commit
24f6a718e0ed6877042db7a2bd6ef43d0ae54277. It configured and built with official
Clang 19.1.7 and failed only frontend, 58/59, after 1.84 seconds. The flushed log
shows structured diagnostics AND value structs completed, followed by
`Frontend begin: receiver methods`. This narrows the failure beyond the initial
summary. Windows Debug passed and all four new string suites passed Release.

Before modifying compiler code, reproduced frontend's same final progress and
SIGSEGV with the existing official-SDK Linux Release binary under exactly 1048576
stack bytes. The standalone `voidc check` reproducer uses the existing 400-call
`point().same()...inspect() + 1` fixture from the earlier Phase 21 investigation.
It also exits 139 under 1 MiB, independently of JIT, GC and other frontend tests.
GDB captured repeating `Semantics::expression -> expression_impl ->
method_expression -> expression` frames. A separate -O3/-DNDEBUG build with debug
information shows failure at compiler.cpp:1156, the next Depth guard construction,
with semantic depth 232, token `same`, offset 1448, and an actual 0x100000-byte
stack mapping. The existing guard is 256, so the native stack runs out first.

Read-only `git archive HEAD^` extracted parent 5b75d87 into the ignored build tree.
Built its actual CLI with the same official LLVM 19.1.7 Release toolchain. The
parent CLI also exits 139 on the same 1 MiB reproducer; with the ordinary larger
Linux stack it returns 1 with exactly VOID1107 and later VOID2003. The new focused
regression likewise fails against that parent CLI with subprocess return -11.
This establishes a pre-existing optimized small-stack defect. Phase 22 increases
some semantic frame sizes and exposes it in the hosted Windows Release job; it
is not evidence that string execution or optimized AOT failed. The earlier Phase
21 hint-recursion correction did not guarantee small optimized dispatcher frames,
and previous local repeated stack gates covered Debug only.

Clang -fstack-usage and disassembly show optimized inlining reintroducing heavy
handler/contextual-conversion temporaries into recursive frames. Before correction,
expression, dispatcher and method frames are 1416, 1576 and 1416 bytes respectively,
about 4.4 KiB per receiver level. Parent figures are 1288, 1576 and 1416. After
correction the current figures are 408, 632 and 1416, about 2.5 KiB per level.
These are Linux compiler measurements, not Windows stack measurements. Run #38
retained no Windows native backtrace or dump; the matching local failure and fix
are actionable evidence, while hosted Windows confirmation remains outstanding.

### Narrow correction and regression

Preserved the existing separate semantic handlers with explicit noinline call
boundaries, including contextual conversion, using the compiler's platform
attribute spelling. The functions remain optimized at the original build level;
no compiler/JIT/AOT optimization level, stack reserve, depth limit, assertion,
recovery rule or language contract changed. No new frontend or rendering path
was introduced. This makes the existing small-dispatch-frame design survive
Release inlining rather than reducing accepted source depth.

Added tests/integration/semantic_depth.py to the existing source_cli suite. An ordinary 16-call chain must still compile. Two
400-call receiver chains, with and without binary contextual hints, must exit 1,
produce exactly one VOID1107 at a `same` token on the correct line, and retain the
independent later VOID2003 at line 10, column 5. Linux compiler children use an
explicit 1 MiB stack; Windows uses its ordinary executable stack. Strict UTF-8
output decoding is retained. Both cases pass with the corrected official Release
CLI and the regression fails with the parent CLI. Existing frontend depth
assertions remain intact, with a flushed checkpoint immediately before the chain.
The hosted Linux frontend small-stack step now covers Release as well as Debug.
DIAGNOSTICS.md records the optimized handler-boundary requirement.

### Separate lowering finding and intermediate test correction

While strengthening the new regression, a proposed valid 240-call constructor
chain correctly hit the existing combined construction-depth guard; it was never
an accepted valid boundary. A 120-call chain passed semantics but then crashed in
`Lowering::expression` under 1 MiB. GDB identified repeated lowering frames, and
the actual parent Release CLI also exits 139 on that valid fixture. This separate
existing defect does not execute in the original malformed 400-call case, which
must fail before lowering. It remains documented in KNOWN_ISSUES.md rather than
expanding this correction into a lowering refactor. The final focused test uses
an ordinary valid 16-call chain plus both original malformed 400-call cases. No
existing test or assertion was removed or relaxed. The candidate-test failure in
the intermediate official Debug/Release and installed sanitizer source_cli runs
is retained, not reported as a pass.

### Local verification

Rebuilt all six existing configured trees successfully with LLVM 19.1.7,
without changing their optimization, SDK layout or RTTI settings. Final full CTest
results use the frozen implementation and finalized regression:

| SDK/configuration | Build seconds | Full CTest | Test seconds |
| --- | --- | --- | --- |
| Installed SDK Debug | 27.99 | 59/59 | 135.25 |
| Installed SDK Release | 56.64 | 59/59 | 72.04 |
| Installed SDK strict sanitizers | 113.70 | 59/59 | 166.90 |
| Official SDK Debug | 31.21 | 59/59 | 110.55 |
| Official SDK Release | 20.16 | 59/59 | 52.07 |
| Official SDK strict sanitizers | 113.31 | 59/59 | 138.52 |

Total: 354/354 suites. Both SDKs' Debug AND Release frontend tests additionally
passed three repetitions each under RLIMIT_STACK soft/hard 1048576, 12/12 runs.
Strict sanitizer settings remained
`ASAN_OPTIONS=detect_leaks=1:halt_on_error=1:exitcode=87` and
`UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1:exitcode=86`, executed outside the
sandbox so leak detection was not blocked by ptrace. No sanitizer was disabled.
The focused CLI regression also passed directly with the strict installed-SDK
sanitizer CLI. Existing normal/O2 JIT, linked AOT, strings, diagnostics, LSP and
runtime checks are included in the 59 suites. GUI editor integration and VSIX
packaging were not repeated for this compiler-stack-only correction.

Final logs, commands, statuses and unchanged source hashes are retained in
`build/phase22-crash/final-validation/`. The preceding `validation/` directory
retains build evidence and intermediate candidate-test failures. Reproduction,
parent comparison, stack-usage reports and GDB logs are under build/phase22-crash,
including release-1mib-gdb.log, analyze-before-gdb.log, compiler-evidence.su,
compiler-after.su and valid120-gdb.log. `git diff --check` passes.

Stopped for owner review. No commit, push, branch change or hosted workflow run
was performed. The correction is locally verified; Windows Release must still
confirm it in the next owner-controlled hosted run. The separate valid-chain IR
lowering issue remains explicitly unresolved.

## Phase 23 architecture investigation: typed arrays and iteration (14 October 2026)

**Design only. No Phase 23 implementation authorized or performed.** The owner
reports Phase 22 LOCKED after Actions run #39 passed all seven hosted jobs, with
59 suites per configuration. This supersedes the pending Windows confirmation
at the end of the preceding entry. Verified the actual workspace and clean Git
state at `95e79ff` before beginning. No Git mutation, push or hosted workflow was
performed during this investigation.

Created `docs/ARRAYS_PROPOSAL.md` for owner and wrench-clanker architectural review.
Read the current Bible, especially Sections 5, 7, 8.1, 26, 31, 32, 34, 35, 36-40,
59 and the P3/RV clarifications, alongside the current value/reference, managed
construction, GC, strings, analysis/tooling and reload contracts. Read the actual
Type/FieldType/arena, reference-shape/capability helpers, parser/semantic/place
model, IR validation, LLVM layout/root/failure paths, heap maps, construct_into
publication and coordinator contracts. Historical phase-guide exclusions are not
assumed to describe current code.

### Findings and recommendation

The Bible commits to growable and fixed arrays, independent empty defaults,
negative indexing, checked direct lookup, element-first/original-index iteration,
and reverse traversal without copying/reordering. It leaves precise negative
bounds, ranges at equal/extreme endpoints, safe resizable borrows, iterator
invalidation timing, API finalization and the reverse remove-current exception
open or recommended rather than locked. Empty-list later-statement inference is
illustrated but not implemented; the proposal explicitly marks a contextual-only
first slice as a bounded recommendation needing review.

The implementation has no source arrays, indexing or collection/range loops.
Existing lexer/type/postfix/assignment logic handles scalar/nominal types and
static fields; bracket source rejects and iteration has an explicit unsupported
path. TypeLayout currently has flat reference offsets, not a repeated-element
layout. ir::Place cannot safely hold an unknown list element as an ordinary field
index. Existing reference facts, precise roots, complete publication, source
failure channel, shared LLVM lowering and generation ownership are reusable;
arrays require deliberate extensions of these same paths.

Recommended Phase 23 endpoint: structural `[t]`/`[t, n]` types, contextual literals
and defaults, checked value reads and selected writes, count/push/remove_at,
array for/reverse and a bounded i32 range slice. Start scalar proofs, then require
strings, managed/interface and reference-bearing values/nested arrays before
completion. Fixed arrays copy inline values; recommend growable aliases share a
stable GC header with replaceable contiguous GC backing. Recommend compact
repeated reference metadata in the current TypeLayout/tracer over a distinct
collector or unbounded per-capacity offset vectors. Header/buffer commit,
zeroed spare slots, exact size arithmetic, allocation failure and old-generation
independence are explicit proof obligations.

A1-A10 identify recommendations requiring approval: structural identity/extents,
zero-length and default behavior, pure owned templates, inference boundaries,
readonly element profiles, safe fixed const-formal borrowing, index/count types
and negative bounds, selected-owner/version checks, iteration item copies and
mutation timing, range edge cases, repeated GC metadata, APIs and resource policy.
A candidate 256 KiB native array-storage budget is explicitly unmeasured and not
approved; stack and constant-template budgets require review/proof. No arbitrary
runtime capacity limit or string resource policy is silently imposed on arrays.

Escaping element references, in-place inline-element mutating method borrows,
reverse current deletion, slicing/get/pop results, predicate/search operations,
nullable arrays and generic/user iteration protocols are deferred or separately
gated, with reasons and alternatives. Existing referenced-object methods and
checked element/subfield writes are distinct from unsafe interior borrows. String
scalar indexing and length APIs retain their previously approved future-only
contract and are not implemented or redefined by array indexing. Phase 24 generics
and Phase 25 collections are compatibility boundaries, not approval to invent
result, hashing or iterator protocols now.

The proposal includes P23-0 through P23-6 implementation checkpoints, independent
IR validation and capability obligations, GC/OOM/resize/reload tests, shared LSP
integration and eventual six local configurations plus seven-job hosted acceptance.
All 59 existing suites/assertions remain required. The known valid receiver-chain
Lowering::expression stack issue stays a separately scoped hardening task; no
correction is folded into this design. If it blocks eventual array acceptance,
report that blocker rather than weakening or excluding new tests.

### Focused baseline verification only

- `ctest --test-dir build/ci-debug -N`: 59 tests.
- `ctest --test-dir build/ci-debug -R '^(frontend|strings_source|reference_values_source|runtime_coordination|analysis|tooling)$' --output-on-failure`:
  6/6 passed, 26.97 seconds. Existing Debug binaries were used without rebuilding
  or modifying implementation. Includes the unchanged frontend semantic stack
  regression and string/reference/runtime/analysis/tooling coverage.
- Six standalone `voidc check` probes for growable/fixed annotations, array literals,
  indexing, iterable loops and ranges each exit 1 with source diagnostics. Bracket
  forms currently report unexpected character, iteration reports not implemented,
  and the range probe encounters unsupported numeric literal lexing. These confirm
  missing syntax, not newly passing array functionality.
- Copied the actual focused LastTest.log and wrote command/result metadata to
  `build/phase23-investigation/focused-baseline.{log,json}`. Probe fixtures/logs and
  `unsupported-baseline.json` are retained in the same ignored directory.

No full matrix, stress rerun, editor packaging or hosted CI was needed for this
documentation-only investigation. No compiler/runtime/tests/build/grammar files
were edited. Stopped for owner and wrench-clanker architectural review before any
Phase 23 implementation.

### Phase 23 proposal refinements after wrench review (14 October 2026)

The owner relayed wrench support for the overall A1-A10 architecture and requested
focused proposal refinements before implementation approval. Updated only
ARRAYS_PROPOSAL.md and this work log. Phase 23 remains design-only; no implementation
approval is inferred from architectural support or recommended choices.

The revised proposal now requires both i32 and i64 numeric ranges at final Phase 23
acceptance. An i32-only checkpoint remains valid temporarily. Contextual literals
make `0..<items.count` an i64 range with an i64 binder; differing already-typed
endpoint widths still require explicit conversion. Tests must cover both widths,
reverse exclusive membership and extreme endpoints without subtracting a cardinality
that may not fit either i64 or u64. Numeric range endpoints are captured once;
resizing items does not change the captured count or create a list iterator.

Fixed-array arguments retain value snapshot semantics at their ordinary argument
evaluation point, before later arguments. Readonly storage may be borrowed only
when alias writes and owner relocation/resize are proved absent; otherwise copy.
Explicit test requirements include later-argument writes, callee writes through a
second owner alias, and a fixed array in backing storage resized during argument
evaluation. Snapshotting inline bytes never deep-clones referenced objects.

Nested selections must reacquire storage from rooted owners after possible resize
or safepoints, then revalidate dependent versions/bounds before descending or
committing. Cached interior addresses cannot cross these boundaries. Invalidated
selections fail rather than silently retargeting or reevaluating source expressions.
An independently captured nested list header keeps its own identity; an obsolete
outer storage dependency must not spuriously invalidate that reference value.

Clarified that every live element in [0, count) is complete and valid, including
all required-reference leaves in nested values/arrays. Zeroed unused GC capacity
is private, inaccessible storage, not a source default for nonnullable references.
Every newly live element must be populated and validated before visible count is
increased; shifts, spare-slot clearing and count changes commit without an
observable or safepoint-visible invalid intermediate source element.

Recorded the remaining recommended choices together for owner approval: permit
zero-length fixed arrays, require contextual empty-literal typing initially, use
homogeneous element capability profiles, detect iterator invalidation at next
access, and extend the existing collector with repeated reference metadata. The
reverse-deletion table now consistently describes next-access invalidation rather
than an eager mutation ban; the special reverse remove-current exception stays
deferred.

The proposed 256 KiB native storage limit is explicitly UNAPPROVED. It must not
be imposed as a compiler limit or acceptance threshold without target-aware sizing
of parameters, returns, copies, alignment, roots and staging, plus relevant ABI and
Debug/Release small-stack evidence and owner review. The plan approves no numeric
cap by implication; the separate receiver-chain lowering issue remains outside scope.

Verification was limited to document consistency, the requested decision/test-plan
cross-checks and `git diff --check`. No compiler tests, full matrix, build, hosted
run, Git mutation or implementation change was performed. Ready for owner review
of the revised decisions before implementation approval.

### Phase 23 implementation authorization and storage preflight (14 October 2026)

The owner approved revised A1-A10 and implementation checkpoints P23-1 through
P23-6. Updated ARRAYS_PROPOSAL.md to record that approval and selected homogeneous
profiles, contextual empty literals, zero-length fixed arrays, next-access
invalidation and repeated collector metadata. Deferred features stay deferred;
future string indexing and the receiver-chain hardening defect remain separate.
The owner explicitly withheld numerical native-storage limits until evidence and
approval, so the former 256 KiB estimate has not been adopted.

Inspected actual backend.cpp root/storage allocation sites before enabling native
fixed arrays. The native lowerer creates separate locals, construction stages,
reference-containing parameter copies and reference-containing instruction homes,
plus Slot and Frame storage. This means element-count bounds alone cannot prevent
stack exhaustion. The source analysis/LSP APIs also lack a common target-native
frame plan; a budget cannot be imposed honestly using an element-size guess.

Built a scratch LLVM 19.1.7 investigation under build/phase23-storage. It emits
actual LLVM pointer-array allocas, root descriptors and Frame storage, and compiles
for Linux x64, Windows x64 and macOS ARM64 at O0 and O2. Target DataLayouts are
queried from the matching Clang. Native Linux executables link the existing Debug
void_runtime and use actual vd_gc_frame_enter/leave/poll plus coordinated collection
while all frames are active. No optimizer disabling, volatile substitute roots,
new collector, array language syntax or receiver-chain fix was introduced. Null
pointer elements model valid nullable root payloads, not unfilled required-reference
source values. Native Slot and Frame size/alignment are confirmed as 32/8 each.

The 24 final target/mode/layout measurements cover total planned static storage
of 64, 128, 256 and 512 KiB across four simultaneous payload homes plus roots/frame.
LLVM-reported frame sizes exceed those planned totals by 24-96 bytes in these
models, with distinct target/mode costs; proposal Section 9 retains the full table.
Windows/macOS were cross-compiled, not executed. These are native frame probes,
not completed array code generation or C++ aggregate-ABI assumptions.

Executed 32 Linux child runs with hard/soft stack limits both 1048576 bytes at live
frame depths 1, 2, 4 and 8 in both generated-code modes. 64 KiB passes all four
depths; 128 KiB first fails at tested depth 8, 256 KiB at depth 4, and 512 KiB at
depth 2. There are 20 passing cases with balanced cleanup and 12 deliberate
SIGSEGV exhaustion cases. The latter are evidence against oversized per-frame
policies, not passing assertions or newly discovered frontend defects. Logs,
IR, native target assembly, exact commands, layouts and result JSON are retained.

Returned a new candidate of 64 KiB total pre-optimization static storage per
array-carrying function for owner approval, including all known homes, roots,
staging/copies and target ABI scratch. It is NOT adopted and is not a recursion or
cross-platform stack-safety guarantee. Any approved policy must be implemented
through a common target-aware storage plan for analysis/LSP/shared lowering and
validated with real array parameter/return ABI and borrow-copy cases. The scratch
probe does not prove those integration obligations. Array-free functions must not
receive an incidental new restriction.

This is the explicitly required numerical-budget decision handoff before enabling
fixed-array feature lowering. P23-1 is not complete and Phase 23 is not delivered.
No production compiler/runtime/tests/build configuration was changed, no Git
mutation or hosted run occurred, and no numerical budget was silently substituted.
The full six-configuration, strict sanitizer, stress, native AOT, Debug/Release
1 MiB and editor acceptance remains required after implementation; this preflight
does not claim or replace that verification.

Focused existing-runtime check after the probes:
`ctest --test-dir build/ci-debug -R '^(runtime_coordination|runtime_generated|runtime_aot_normal|runtime_aot_optimized)$' --output-on-failure`
passed 4/4 in 3.29 seconds. Inventory remains 59. Document consistency and
`git diff --check` pass. No full-matrix acceptance result is claimed at this gate.

### 2026-10-14: Phase 23 manual native stack-probe workflow

Owner requested investigation only before any numerical storage decision or array
implementation. Added `phase23-stack-probe.yml` with only workflow_dispatch and
native Ubuntu x64, Windows x64 and macOS ARM64 jobs using LLVM 19.1.7. Reused the
existing pinned SDK installer/cache. Normal seven-job `ci.yml` remains byte-for-byte
unchanged. No Git mutation, push or hosted dispatch occurred.

Added reproducible files under `tools/ci/stack_probe/`. The isolated CMake project
builds only six existing runtime files plus the native harness, with generated LLVM
objects at O0/O2; it never configures the compiler root or invokes CTest. Probes
retain the earlier four live pointer-array homes and real root/collection ABI for
64/128/256 KiB, with nested depths 1/2/4/8/16. Null elements represent valid nullable
roots, not incomplete required-reference values. Native ABI checks guard execution.

Each child runs under explicit worker-stack controls: POSIX 1 MiB supplied mapping
with 512 KiB low guard, high guard page and separate 64 KiB signal stack; Windows
1 MiB reservation checked through VirtualQuery and 64 KiB requested exception
headroom. README and proposal explain that these are not equivalent usable stack
conditions or main-thread RLIMIT_STACK. Only recorded guard faults or actual Windows
stack-overflow exceptions qualify as expected failure. Six unrelated-fault controls
must be rejected as overflow. Other faults/setup errors/timeouts fail the experiment.

Final local native command:
`python3 tools/ci/stack_probe/run.py --llvm-bin /usr/lib/llvm-19/bin --output build/phase23-stack-probe-final`
passes all 30 depth cases: 18 successful collections/cleanups, 12 verified low-guard
exhaustions and six successful unrelated-fault controls. At both modes 64 KiB passes
through depth 8 and exhausts at 16; 128 KiB passes through 4 and exhausts at 8/16;
256 KiB passes through 2 and exhausts at 4/8/16. Linux O0 frame/stride measurements
are 65560/65568, 131096/131104 and 262168/262176 bytes. O2 values are 65576/65584,
131112/131120 and 262184/262192. The external observer records consistent frame
spacing without disabling optimization. Local artifacts retain exact native evidence.

`python3 -m unittest discover -s tools/ci/stack_probe -p test_runner.py` passes four
focused evidence checks, including rejection of outside-guard addresses, Windows
access violations, bare exit/signal claims, missing/malformed evidence and wrong
stack extents. The manual workflow runs these checks before the native experiment.
Artifacts retain commands, source/header hashes, version/target, frame diagnostics,
assembly, stack bounds, per-child JSON/logs and concise summaries for 14 days.

Windows/macOS execution remains pending owner push/dispatch. Earlier cross-target
compilation is not execution. No compiler limit, array feature, receiver-chain fix,
full compiler build or full 59-suite verification is included. All existing suites
and normal CI are preserved. Proposal Section 10 records the Linux results and
remaining platform evidence. Stopped for owner review before any hosted action.

Workflow YAML parses successfully; inspection confirms only workflow_dispatch and
three native runner entries. Compared normal CI bytes against HEAD, SHA-256
`a571baeb838fea17b3baa3d80f759739cd2d2295313357a75ed5eae6a5cb5400`, unchanged.
`git diff --check` passes. No full compiler matrix was run for this experiment.

### 2026-10-14: Native probe run #1 portability corrections

Inspected all three hosted job logs for
[probe run #1](https://github.com/Shmellyorc/Void.Language/actions/runs/38656555339),
commit `4e905b5a349ad7512bb996763238d22a2966e187`, and downloaded the Windows
and macOS artifacts into ignored `build/phase23-probe-run1`. Ubuntu job
116167589699 passed all 30 depth cases with the previously recorded thresholds.
Windows job 116167589376 failed O0 CMake configuration before any executable ran.
macOS job 116167589596 executed natively on ARM64 with Clang 19.1.7; its results
contain 18 exit-70 normal-return cases and 12 exit-90 verified guard overflows.
All six macOS unrelated-fault controls passed. Small-depth logs report successful
collection and balanced runtime cleanup, followed by the generic alternate signal
stack cleanup error. These are failed harness cases, not accepted platform passes.

Windows `configure-O0.log` identifies CMakeLists.txt:33 add_executable reparsing
`D:\a\Void.Language\Void.Language\build\phase23-stack-probe\O0\objects/frame-64.o`
and rejecting the `\a` escape. Native compiler detection succeeded as Clang 19.1.7
with the MSVC command-line interface. The runner now supplies compiler and object
directory cache paths through Path.as_posix(), using explicit CMake PATH types.
Other native process paths, LLVM target selection, compilation and Windows stack
checks are unchanged. A focused regression uses PureWindowsPath with a drive,
`\a`, spaces and nested directories; a real CMake second-parse check reproduces
the original invalid escape and accepts the corrected argument on local Linux.
This validates path serialization, not Windows configuration or execution.

The former POSIX cleanup sent a zero-initialized descriptor with SS_DISABLE and
combined sigaltstack and munmap with short-circuit OR. Apple's
[UNIX03 wrapper source](https://github.com/apple-oss-distributions/Libc/blob/71bbe350ab79eef58113991d817ccc6165061a64/compat-43/sigaltstk.c)
rejects a descriptor smaller than MINSIGSTKSZ with ENOMEM before checking its flags
or entering the kernel. This identifies a concrete zero-size disable defect in
our request. Run #1 did not capture errno or the individual operation, so ENOMEM
is source-supported diagnosis, not a fabricated native errno observation.

Corrected cleanup preserves the installed descriptor size when disabling, queries
and verifies disabled/not-active state, then unmaps the alternate stack. Separate
JSON events record each syscall return and immediately saved native errno. Failures
throw with operation, numeric errno in JSON and error text on stderr, retaining
exit 70. A failed disable/query never unmaps possibly registered storage. Success
is now emitted after cleanup, and POSIX success classification additionally requires
all three successful cleanup records. No failure classification was relaxed.

Added one isolated cleanup-check child per POSIX mode. It records the old zero-size
disable result, numeric errno and ENOMEM name directly on the executing OS, then
requires the corrected lifecycle. Only the deliberately invalid diagnostic request
may return ENOMEM; any error from real cleanup remains fatal. If the old call succeeds,
the control reinstalls the stack before checking the corrected cleanup. This will
isolate the exact macOS operation/error in the next hosted artifacts without
silently retrying or suppressing failures in normal probe runs.

Focused local verification:

- `python3 -m unittest discover -s tools/ci/stack_probe -p test_runner.py` passes
  six tests, including real CMake path reparsing and rejection of cleanup failure
  even after a successful-collection event, missing cleanup evidence and failed
  unmapping. Existing crash-evidence tests remain intact.
- `python3 tools/ci/stack_probe/run.py --llvm-bin /usr/lib/llvm-19/bin --output build/phase23-stack-probe-portability`
  passes 30 Linux native depth cases: 18 normal completions and 12 verified guard
  overflows, plus six unrelated-fault controls and two successful lifecycle controls.
  On Linux the old zero-size disable returns 0/errno 0; reinstall, corrected disable,
  disabled-state query and munmap all return 0/errno 0 at both O0/O2.
- Linux thresholds and LLVM frame/observed stride measurements are unchanged from
  the previous local probe. Verified 1 MiB stack bounds, 512 KiB guards, alternate
  signal-stack size, precise root checks and unexpected-crash rejection remain.

Windows/macOS corrections require another native hosted run. No local result is
claimed for those platforms and no hosted dispatch occurred. Updated the harness
README with lifecycle evidence and run #1 limitations. No production compiler or
runtime source, normal CI, suite inventory or numerical storage policy changed.
No full suite, Git commit or push was performed. Stopped for owner review.
Final byte comparisons confirm both workflows are unchanged; comparison of old/new
Linux result JSON confirms identical thresholds, frame sizes and observed strides.
`git diff --check` passes.

### 2026-10-14: Probe #2 CMake path-regression policy baseline

The owner reports probe #2 failed the CMake path regression on Ubuntu and Windows,
while macOS ARM64 passed the complete native experiment. Wrench reproduced the
regression failure with CMake 3.31.6: the generated path.cmake omitted a policy
baseline, leaving CMP0010 unset and allowing the unsafe escape to produce a warning
with exit zero. This hosted/reproduction evidence is supplied by the owner; it is
not a new local Windows or macOS verification claim.

Added `cmake_minimum_required(VERSION 3.24)` before `cmake_language(EVAL CODE ...)`
in the generated regression script, matching the actual probe project's minimum
and policy baseline. Preserved the unsafe-path nonzero-exit and invalid-escape
assertions and the normalized-path zero-exit assertion without modification.

`python3 -m unittest discover -s tools/ci/stack_probe -p test_runner.py -v` passes
all six regression tests locally with CMake 4.3.4, including the real CMake unsafe
path failure and normalized path success. `git diff --check` passes. Local CMake
3.31.6 was not used; that version's reproduction is the wrench-supplied evidence.
Only the generated regression script and this log entry changed. Production code,
probe measurements, thresholds, runner and workflows remain unchanged. No native
stack experiment or full suite was rerun for this test-only policy correction.
Broader documentation cleanup remains deferred until after hosted testing.
No commits, pushes, remote changes or hosted dispatch occurred. Stopped for review.


### 2026-10-14: Probe #3 independently verified; 64 KiB approval

Read all three job logs and downloaded all three artifacts for
[run #3](https://github.com/Shmellyorc/Void.Language/actions/runs/38660215692),
commit ea8685180750f69b1dc4d182b7ac5940ddc6c9bd. Linux x64 job 116179441236,
macOS ARM64 job 116179441602 and Windows x64 job 116179441784 all succeed.
Each platform passes six regressions, 18 successful native depth cases, 12 verified
stack overflows and six unrelated-fault controls. JSON confirms native targets,
LLVM 19.1.7, exact 1 MiB worker extents and cleanup. Linux/macOS pass two lifecycle
controls each; macOS independently records old disable errno 12/ENOMEM and corrected
cleanup success. Section 10 now retains all six frame/stride measurements per
platform, corrects its stale pending claims and six-test count. Artifacts are
retained locally in ignored build/phase23-probe-run3.

Recorded owner/wrench approval of 65536 bytes per array-carrying function, with
explicit target layout, parameter/result homes, copies, roots, staging, alignment
and known ABI overhead accounting. This is not a maximum allocation, save-file size
or universal stack guarantee. Implementation remains subject to the real ABI/storage
proofs, with P23-1 only currently authorized. No compiler limit is enforced from
hand-generated payload estimates. P23-2 through P23-6 remain review-gated.


### 2026-10-14: P23-1 scalar fixed-array foundation and storage contract

Implemented only the P23-1 source/analysis/IR checkpoint after recording native
probe #3 evidence and the owner's 65536-byte approval. Existing frontend, type
arena, pure constant evaluator, selected-place lowering and independent IR
validator remain the single paths. No native arrays, ranges or iteration were added.

Changes:

- Added structural array descriptor kinds and canonical identity from exact element
  type and evaluated extent. A descriptor has one weak element edge and an extent,
  never synthetic nominal element fields. Existing TypeArena ownership retains
  query/IR Types and their element definitions without strong descriptor cycles.
  Active source storage is scalar fixed arrays; growable/nested/reference-bearing
  storage explicitly diagnoses as deferred to subsequent checkpoints.
- Added brackets, contextual fixed literals, exact length checks, zero-length
  arrays, zero defaults, fields, parameters/results and value-copy IR through the
  existing parser and semantic pipeline. Numeric/bool elements reuse contextual
  numeric typing. No implicit widening, null elements, list inference, string
  indexing or deferred builtin API was introduced.
- Kept extent expressions as owned AST until declaration discovery. Reused module
  constants, imported/root visibility, dependency states/cycle diagnostics and
  bounded semantic work. Forward extents and pure value-constant fields work;
  runtime/local-const extents, negative/noninteger/oversized extents and cycles
  reject with source diagnostics. Array multiplicity participates in the existing
  65536 expanded-shape and 128 depth guards before default allocation. A regression
  proves extent parsing cannot reset the enclosing 512-node expression guard.
- Added checked fixed construction/read/update IR. Independent validation derives
  element types and exact arity, rejects noninteger indexes, forged extents/strides,
  wrong values, invalid structural descriptors and unsupported nonscalar storage.
  Shared unsigned-bit normalization covers all integer widths, signed MIN,
  unsigned MAX, -1/-n and empty arrays without narrowing or signed-negation UB.
  Pure constant indexing executes these checks during analysis; runtime accesses
  remain checked IR with original source snapshots.
- Reused selected-owner places for exact-once evaluation. Reads acquire current
  array bytes after index effects. Writes check before RHS, snapshot compounds,
  preserve the original selected owner and reacquire current inline bytes after
  RHS effects. Existing capability and construction visitors also traverse index
  effects. Scalar arrays introduce no GC/reference-authority leaves. Const-formal
  metadata remains distinct for later snapshot/borrow proofs; no borrowing is
  implemented or presumed safe at this checkpoint.
- Added NativeStoragePlanner to void_core as the common inventory/aggregation
  contract. It requires explicit target identity/DataLayout, a target value-layout
  provider, actual storage extents/alignment and complete accounting for parameter,
  result, local, instruction, copy, root/frame, staging and known ABI homes.
  Checked multiplication/alignment/addition and the approved 65536-byte policy
  reject incomplete/oversized inventories. There is no host-size fallback or LSP
  calculator. Arithmetic tests use explicitly synthetic layouts, not invented
  platform measurements. The actual LLVM/ABI inventory producer and target-context
  plumbing into shared analysis/tooling remain P23-2 prerequisites. The shared
  LLVM path explicitly rejects array types/operations until those proofs exist.

Verification on LLVM 19.1.7:

- Added arrays_foundation, linked to the existing core, with positive/negative
  source cases, forward/imported ownership and constants, finite/default/zero
  layouts, exact diagnostic operand spans, readonly aliases/formals, all scalar
  element and integer index widths, CFG/evaluation order, independent forged IR,
  retained arena ownership and incomplete/overflowing storage inventories.
- Final full Debug CTest passes 60/60 in 66.24 seconds, including all original
  59 suites. The initial full 59-suite Debug check also passed in 75.87 seconds.
  Inventory comparison against the untouched official Debug baseline confirms no
  original suite was removed or renamed; arrays_foundation is the sole addition.
  Logs: build/p23-1-debug-all-final-tests.log and build/p23-1-debug-tests.log.
- Final focused Debug and Release checks pass 6/6 each: arrays_foundation, frontend,
  numeric, analysis, modules and tooling. Debug LSP positions/protocol/demo pass
  3/3. Logs: build/p23-1-debug-focused-tests.log, build/p23-1-release-tests.log and
  build/p23-1-lsp-tests.log. These are not a six-configuration acceptance claim.
- arrays_foundation and the original frontend suite pass with a 1048576-byte stack
  in both Debug and Release. Frontend logs are build/p23-1-debug-frontend-1m.log
  and build/p23-1-release-frontend-1m.log. This does not test or fix the separate
  valid receiver-chain lowering defect.
- Strict ASan/UBSan arrays_foundation passes with detect_leaks=1,
  strict_string_checks=1, halt_on_error/abort_on_error and UBSan halt/stacktrace
  enabled, in 0.13 seconds. Initial sandbox execution failed at LeakSanitizer
  thread inspection under ptrace, so the identical strict run was executed outside
  that restriction with authorization. No sanitizer or leak checks were disabled.
  Log: build/p23-1-san-tests.log.
- CLI check accepts a scalar-array source; both run/JIT and emit/AOT reject it with
  the explicit P23-2 prerequisite message. Log: build/p23-1-native-gate.log.
  No native fixed-array execution is claimed. Frozen implementation hashes are
  in build/p23-1-source-hashes.json. Both workflow files remain byte-for-byte
  unchanged; no production runtime file was changed.

Proposal Sections A10/9/10/11 now distinguish historical proposals, verified native
runs, approved numerical policy and the P23-1 implementation boundary. Updated the
probe README and summary wording so new reports refer to the recorded approval
instead of incorrectly claiming the budget remains unapproved. The probe's stack
controls, measurements, thresholds and execution code are unchanged.

Limitations/review handoff: actual target ABI home accounting and shared target
configuration must be proved before native fixed-array lowering. Global constant-
array pool policy, nonscalar/nested/growable storage, GC maps/publication, resize,
iteration/ranges, indexing of strings and later tooling/editor acceptance remain
outside this checkpoint. No guessed global array-pool cap was adopted. No full
six-configuration matrix, new hosted run or P23-2 work is claimed. No commits,
pushes, branches or remote mutations occurred. Stopped for boss/wrench review.
Final probe-harness regressions still pass 6/6 after the summary-text correction.
Frozen implementation hashes match the verified files and `git diff --check`
passes. Generated frontend fixtures from small-stack checks are retained under
ignored build/p23-1-frontend-1m-fixtures, outside the reviewable source changes.

## Phase 23 P23-2: native storage ABI proof gate, 14 October 2026

Boss/wrench report run #40 passed seven hosted jobs with 60/60 suites per
configuration and lock P23-1. P23-2 authorizes native scalar fixed arrays, iteration,
ranges, const-formal snapshots/borrowing and complete shared target accounting,
with an explicit stop if the storage or borrow proof cannot be established.

Stopped at the storage prerequisite, before production changes. The pinned
backend currently creates raw LLVM value parameters/results, while P23-1 rejects
array types before native lowering. Simply mapping structural arrays to ArrayType
would make LLVM's aggregate call legalization introduce storage absent from a
DataLayout-only payload inventory. NativeStoragePlanner's complete-role checklist
cannot certify a producer that omits that storage. No new producer or separate
LSP estimator was installed and no numerical ABI allowance was guessed.

Added a reproducible standalone investigation under tools/ci/array_abi_probe,
using LLVM 19.1.7 opt/llc and target-derived DataLayouts. Caller/callee live in
separate LLVM modules; a separately compiled C host reaches only a scalar adapter.
It verifies all elements for two seeds and exact helper counts. No LTO, noinline,
volatile, disabled optimization or production source recognizer is involved.
Normal means the existing unoptimized-IR path; O2 uses default<O2>. Both use the
AOT default machine-code level, llc -O2. This is not a machine -O0 experiment.

Verification: all 12 target/size/mode pairs completed, with 24 independently
verified input modules and 24 MIR frame measurements. Linux produced four pairs of
independently compiled objects and four separately linked executables. All eight
native process runs passed, four with the inherited limit and four with hard/soft
RLIMIT_STACK=1048576 before exec; each validates two round trips. PIC object frame
warnings match all eight corresponding Linux MIR measurements. Windows x64 and
macOS ARM64 were cross-compiled only, with no native execution claim. These are
standalone LLVM AOT experiments, not VOID JIT/AOT acceptance or hosted results.
The normal/O2 frame table is in ARRAYS_PROPOSAL.md Section 12. Commands, version
logs, process output, generated IR/MIR, objects, binaries and results
are retained in build/p23-2-abi-proof; results.json and commands.json are the concise
machine-readable evidence. README.md provides the tracked reproduction command.

The 4096-element Linux caller has two explicit 16384-byte homes, but its native
frame is 81880 bytes. MIR identifies an additional 16384-byte result home,
32736-byte maximum outgoing call area, scalar spills and saved-register storage;
callee fixed incoming slots are outside its local stackSize. Those reported fields
can overlap and must not simply be added. Raw call legalization, scalar stack-slot
sizes and optimization-sensitive spills must be covered by the production proof,
including all scalar kinds, zero extents, array-containing structs, root frames,
staging and snapshot fallbacks. The experiment deliberately does not claim that
coverage. The approved 65536-byte policy is unchanged and is not shown unworkable.
P23-1 native stack-probe evidence remains valid for its own, different signature.

ARRAYS_PROPOSAL.md Section 12 records measured frames, the incomplete proof and a
review recommendation to investigate explicit internal array value/call homes
versus retaining raw aggregates with target machine auditing. Either must preserve
array-free ABI, ordinary snapshots, the preoptimization inventory and shared
analysis/LSP/JIT/AOT target configuration. No internal ABI alternative was adopted.
Const-formal borrow/fallback proof work remains behind this earlier gate; no
readonly or aliasing defect in locked P23-1 is asserted.

Actual validation of the unchanged compiler baseline:

- Debug arrays_foundation/tooling/analysis/frontend pass 4/4 in 10.22 seconds;
  Release passes the same 4/4 in 6.11 seconds. Logs retained as
  build/p23-2-debug-baseline.log and build/p23-2-release-baseline.log.
- Debug and Release arrays_foundation pass with a 1048576-byte process stack limit.
  CLI check still accepts a scalar fixed-array source; JIT and AOT still reject it
  at the explicit P23-2 prerequisite gate. Evidence: baseline-gates.json under the
  probe output. No native VOID array feature is claimed from the standalone IR.
- CTest inventory remains exactly 60. No existing suite registration, assertion,
  production compiler/runtime file or workflow changed. Probe Python compilation
  and C11 -Wall/-Wextra/-Werror/-pedantic syntax checks pass. git diff --check passes.

P23-2 implementation and acceptance are incomplete: native checked reads/writes,
iterable/range CFGs, proved borrowing, full storage enforcement, JIT/AOT agreement,
strict sanitizer and native feature regressions are not delivered. No full matrix
or new hosted run was attempted after this proof stop. No growable/reference array,
P23-3 through P23-6, receiver-chain or string-indexing changes. No commits, pushes,
branches or remote changes. Stop for boss/wrench review as instructed.

## P23-2 explicit address ABI storage investigation, 14 October 2026

Investigation only, after wrench accepted the Section 12 proof gate. The approved
65536-byte per-function policy remains unchanged. No production array/ABI,
compiler/runtime, LSP, test registration or workflow was modified. P23-1 remains
owner-locked; proposal status now reflects run #40 and the present investigation
rather than the stale P23-1-only authorization wording.

Extended the existing tools/ci/array_abi_probe harness with raw/address selection,
explicit result/snapshot/private callee homes, native semantic checks, a pinned-MIR
storage auditor and nine focused regressions. The original raw IR generator and
both seed/every-element/exact-helper-count assertions remain intact. The additional
C assertions run only after those original checks. All caller/callee objects stay
independent; the combined-module inlining experiment is additional coverage.
No noinline/volatile, disabled optimization, larger stack or heap substitution.

The address prototype passes plain result/snapshot/failure pointers, with three
caller array homes and one private callee home, no byval/sret or aggregate values
across that boundary. Scalar adapter ABI remains i32(i32, ptr). LLVM memcpy
intrinsics optimize normally. Later-argument owner mutation cannot change the
first snapshot; callee mutation cannot change that snapshot either. Failure tests
check evaluation order, later-argument failure before invocation, callee failure
after a partial result write, no result consumption on failure and bounded trace
propagation using the existing failure channel's shape. Source locations and
production diagnostics/root behavior are not claimed verified by this toy channel.

Actual frame results for 4096 i32 elements, payload 16384 bytes:

| Target | Raw caller normal/O2 | Address caller normal/O2 | Address callee normal/O2 |
| --- | ---: | ---: | ---: |
| Linux x64, native | 81880 / 81880 | 49192 / 49192 | 16408 / 16408 |
| Windows x64, compile-only | 98344 / 98344 | 49224 / 49224 | 16440 / 16440 |
| macOS ARM64, compile-only | 81904 / 81920 | 49200 / 49200 | 16416 / 16416 |

Normal means unoptimized LLVM IR; O2 uses default<O2>. Both use the existing AOT
machine default, llc -O2/PIC, not machine -O0. Address measurements are new. The
large raw frames reuse frozen Section 12 artifacts after byte-for-byte input IR
checks across all 12 historical cases. Fresh raw 1024-element code generation
matches all six prior target/mode pairs. All eight old native raw executions were
also replayed, including four with a 1 MiB limit. Cross-compilation is never counted
as Windows/macOS execution.

Added one small native C++ helper built against the matching LLVM 19.1.7 development
SDK, not the compiler project. It queries actual DataLayout i32 size/alignment,
pointer size and TargetFrameLowering stack alignment/local-area offset. These agree
with the target compiler. For the sampled nonleaf x64 profiles the entry-area
prefix outside getStackSize is 8 bytes; on ARM64 it is 0. This matches prior native
frame/stride evidence and is not an invented fixed copy/spill allowance. Other
target shapes remain unsupported until proved. The README records the SDK/header/
library requirement and Linux-only driver limitation.

For supported fixed/nonoverlapping homes, the auditor retains the original
unrounded preoptimization inventory P, matches represented object bytes R, reads
the actual machine frame F and target entry prefix E, then charges
align_up(P + F - R + E, target stack alignment). Call areas/spills/padding already
inside F are not added again. Unknown homes, unsupported incoming ownership,
realignment, opaque/dynamic shapes and tail calls receive no budget verdict.
This is a limited MIR proof experiment, not the proposed production implementation.

At 5440 elements, caller charges are 65328/65360/65328 bytes for Linux/Windows/macOS,
within policy. At 5461, the original caller homes are 65532 bytes (aligned 65536),
but charges become 65584/65616/65584; all three are flagged over-budget. The compiler
limit is not enforced by this tool; those samples execute only as investigations.
The policy remains per function, not an array-allocation or entire-call-chain cap.

Actual final verification with LLVM/Clang 19.1.7:

- Address experiment completes 24 target/extent/mode pairs and 48 principal machine
  frame measurements. Additional semantic modules are measured on all three targets.
  Native Linux independent AOT passes 16/16 runs: eight inherited-stack and eight
  hard/soft RLIMIT_STACK=1048576 before exec. This is a main-process limit, not the
  older mapped-worker experiment. Each process validates both original round trips
  and all three additional semantic modes.
- Eight additional standalone Linux lli ORC executions pass with generic CPU,
  machine O2 and independent IR modules. All 32 actual JIT frame warnings match
  the corresponding four AOT/MIR functions. ORC runs use the inherited stack limit.
  Production VOID host-feature JIT configuration is unchanged and not certified.
- All native PIC object frame warnings agree with MIR. There are no accepted
  unexplained crashes. Raw fresh native checks preserve both original assertions.
- Nine audit/semantic regression tests pass. They catch real hidden raw result
  homes, missing/inexact plans, an understated frame, unknown allocations and
  unproved stack shapes. Four deliberately broken normal/O2 protocols, unsafe
  borrowing and delayed snapshots, exit exactly 1 through the C assertions.
- An actual dead home is retained at normal and deleted by O2; both retain its
  16-byte policy charge. An initial test incorrectly expected deletion at both
  modes; inspecting the real MIR corrected that assumption while retaining the
  preoptimization-budget assertion.
- The ordinary combined-module O2 experiment runs natively and passes the C value
  checks, with a 49176-byte entry frame. It deletes snapshot and introduces a
  16384-byte home.i from the inlined callee. The auditor rejects the untracked
  provenance rather than crediting the smaller frame or suppressing inlining.
- Existing arrays_foundation passes Debug (0.09 s) and Release (0.02 s). CTest
  inventory remains 60. Python compilation, strict C host builds and diff checks
  pass. No full compiler matrix or hosted CI was run for this investigation.

Evidence: build/p23-2-address-proof-final contains complete.json, results.json,
commands.json, target-profiles.json, per-function audit JSON, IR/MIR and binaries.
The audit-regressions directory contains the real positive/negative optimized
modules, native negative controls and inlining-audit.json. Logs are
build/p23-2-address-proof-final.log, build/p23-2-address-audit-tests.log and
build/p23-2-raw-recheck.log; fresh raw artifacts are build/p23-2-raw-recheck. The
raw-baseline-native-replay.json records historical binary re-execution. README.md
includes reproduction from tracked files without requiring these old artifacts.

Recommendation in ARRAYS_PROPOSAL.md Section 13: prefer explicit address-based
argument/result handoff with owned snapshots as the correctness path, preserving
array-free ABI. Share one exact target/profile preflight and audited emitter with
analysis/LSP, AOT and an ORC compile callback. Use MachineFrameInfo/AllocaInst
provenance and actual target frame lowering, not the Python parser, a second LSP
calculator or fixed allowances. Pipeline insertion/late target changes, clone/
inlining/reuse lineage and incoming/outgoing ownership must be proved first.
The Windows five-argument semantic callee's 8-byte incoming slot remains explicitly
uncertified; its 120-byte frame alone is insufficient. GC/root/staging completeness,
managed-owner liveness, result publication, readonly/alias borrow verification and
reload stability remain separate obligations. Scalar i32 samples do not prove
those or other scalar kinds/zero extents.

Investigation is ready for boss/wrench review; the production proof gate remains
open. No production arrays, P23-3 through P23-6, unrelated receiver-chain hardening
or string-indexing implementation. No commits, pushes, branches or remote changes.

## P23-2 C++ MachineFunction storage proof, 14 October 2026

Scope: focused implementation proof approved after review of 47ebbeb. The existing
explicit-address reproducer is reused. No production array lowering, ABI, runtime,
LSP, normal CI or later checkpoint changed. The policy remains 65536 bytes. This
entry advances the earlier MIR investigation, not full P23-2 implementation.

Added tools/ci/array_abi_probe/machine_emit.cpp, machine_proof.py and test_machine.py.
The standalone emitter uses the installed pinned LLVM 19.1.7 SDK. Before IR
optimization it freezes original static AllocaInst identities, selected-target
sizes/alignment and homes. WeakVH does not follow RAUW to replacements; P is never
recomputed from optimized IR. Actual MachineFunction passes inspect early call
areas, post-PEI placement, the complete late machine pipeline and post-AsmPrinter
state. A final LLVM MC object decoder compares emitted SP writes and call opcode
sequences with the certificate. Accepted objects are SHA256-bound and delivered
only after all checks via a private temporary file and rename. Existing delivery
paths are refused; incomplete/over-budget cases return 2 without a delivery object.

The late gate is after TargetPassConfig::addMachinePasses and before addAsmPrinter,
following the pinned LLVMTargetMachine.cpp standard pipeline, with machine
verification enabled. PEI alone is too early. A separate standard llc
-print-after-all witness identifies Control Flow Optimizer (branch-folder) as a
real later change: the Linux normal semantic caller goes from three epilogues to
one. C++ observes 20 SP-writing instructions after PEI versus 10 at the gate;
F remains 232. Its callee changes 15 to 10 with F=104. Windows normal caller and
callee similarly change 20 to 10 and 12 to 8, with F unchanged at 280 and 120.
The late snapshots, not earlier instruction sequences, are certified.

Final MC encoding also changes x86 ADD/SUB imm32 instructions to imm8 forms. The
checker permits only these encoding families with identical decoded registers
and signed immediate values; it does not ignore differing stack effects. Mach-O
section anchors appear as function symbols in LLVM's generic object API. An alias
is accepted only if its section/address/byte extent equals a certified function;
other emitted functions fail. These are explicit bounded object-format/encoding
cases, not suppressed audit failures.

R requires the actual original live AllocaInst pointer, exact machine extent and
sufficient alignment, without duplicate or overlapping credits. Spill/save bytes
stay in F. Dynamic, opaque, tail-call, realigned, outside-frame/red-zone and
unmatched-home shapes fail. Actual lifetime-based coloring merges two original
256-byte homes into one object on all platforms/modes. The conservative prototype
awards no R when lifetime markers allow reuse, including optimization-introduced
markers: P=512, R=0, F/C=264/784 Linux, 296/816 Windows, 288/800 macOS. It does not
claim merged lineage from the one surviving pointer. Deleted dead homes retain
their 16-byte policy charge. No subtraction is inferred from names or sizes alone.

Inlining is enabled normally. The original address pair combined at O2 produces
entry F=49176 Linux, 49208 Windows, 49184 macOS; original P remains 49152. New homes
lack proved original provenance and lifetime reuse receives R=0, so delivery fails.
An originally array-free wrapper also acquires an actual inline array allocation;
its original P=0 is not an exemption, and it is rejected with R=0. Normal versions
pass. Production clone/merge lineage remains an open integration obligation.

Windows five-argument ownership is resolved for the exact probe C signature
void(ptr,ptr,i32,i32,ptr). Real call-frame setup for checked_roundtrip is 40;
register-only helpers use 32. The late caller stores the fifth argument at SP+32.
The callee fixed slot is 8 bytes at LLVM frame offset 32, resolved by frame lowering
to post-prologue RSP+160, or entry SP+40, with F=120. Caller pre-CALL SP+[0,32) is
shadow space and [32,40) is the fifth argument. Callee entry SP+[0,8) is the return
slot E=8; the caller area begins at entry SP+8. The incoming argument is already
owned by the caller's outgoing 40 bytes in F, and is not a second callee local.
The callee's own outgoing 32 bytes for touch remain in its F. A previously
certified caller object, hash-bound local certificate, exact signature, actual
40-byte setup and late SP+32 store are required. Missing ownership, shadow-only
proof, missing-store proof and stale object hash all block delivery. This is not
a general classifier for Windows aggregates, indirect calls, varargs or exports.

Both normal and target-aware O2 use generic CPU, empty explicit feature string,
PIC and machine Default/O2. DataLayout must match the selected target's compiled
witness. For a 4096-element payload, caller F/C is 49192/49200 Linux,
49224/49232 Windows, 49200/49200 macOS; callee F/C is 16408/16416,
16440/16448 and 16416/16416. At 5440 elements all pass, with caller C
65328/65360/65328. At 5461 all fail, with C=65584/65616/65584. Over-budget callers
are not delivered, linked or executed. No constant ABI overhead is invented.
The Windows semantic caller is F=280 at explicit generic CPU, versus the earlier
empty-CPU llc F=264; independent llc -mcpu=generic confirms 280. Its C is 288.
The semantic caller/callee Linux F/C is 232/240 and 104/112; macOS is 240/240 and
112/112; Windows callee is 120/128. Do not mix CPU profiles in cached certificates.

Actual final verification:

- machine_proof.py completes 18 target/extent/mode cases and 46 additional controls,
  issuing 94 C++ auditor invocations over 106 MachineFunctions: 60 module objects
  certified/delivered and 34 rejected without delivery objects. Source snapshots
  and hashes are captured before compiling and checked unchanged afterward.
- Native Linux passes eight independent AOT executions, four inherited-stack and
  four with both RLIMIT_STACK limits set to 1048576 before exec. All original
  per-element/two-seed/call-count assertions and semantic mutation/evaluation-order/
  failure-channel assertions are retained. Four standalone ORC loads of certified
  objects also pass under 1 MiB. The C host adapter is compiled separately. This
  verifies the prototype object gate, not production ORC IR compilation or reload.
- Windows x64 and macOS ARM64 compile and decode at both modes. No Windows/macOS
  native or hosted execution is claimed. Six over-budget principal callers,
  original/array-free inline clones, omitted homes, missing incoming ownership,
  twelve late frame/SP mutations and two post-machine encoded-byte mutations all
  fail closed. The encoded-byte controls leave post-AsmPrinter machine snapshots
  unchanged and are rejected independently by final object decoding.
- All 14 new regression tests pass (1.35 s), including 48 independent standard
  llc -mcpu=generic frame measurements matching the C++ records. Tests additionally
  run six bad Windows proof cases: wrong outgoing extent, absent store evidence,
  and stale object hash at normal/O2. Independent .independent.o files are frame
  witnesses, not gate deliveries; no failed certificate is linked or executed.
- All nine pre-existing MIR/value-protocol regressions pass (1.81 s), preserving
  hidden raw-result checks, invalid plans, actual inline unknown homes and unsafe
  borrowing/delayed-snapshot negative controls. Python compilation and diff checks
  pass. Existing arrays_foundation passes Debug (0.09 s) and Release (0.03 s).
  CTest inventory remains 60. No full compiler matrix or ordinary CI was run.

During development, an initial SP fault fixture only selected large prologue
immediates and did not perturb Windows's register-based allocation. Its unexpected
success correctly failed the harness assertion. The fixture now changes a real
SP-defining instruction with an encodable immediate on each profile; all twelve
late frame/SP negatives are actually rejected. No production checks were relaxed.

Final evidence: build/p23-2-machine-certificate-reviewed with complete.json,
results.json, controls.json, commands.json, sources/, sources.sha256, per-function
certificates including after_pei snapshots, optimized IR, disassembly and objects.
Logs: build/p23-2-machine-certificate-reviewed.log,
build/p23-2-machine-certificate-tests.log and
build/p23-2-machine-prior-regressions.log. late-pass-trace.log and
late-pass-transitions.json identify the independent standard branch-folder change.
The README provides fresh reproduction from tracked sources, without historical
build artifacts. ARRAYS_PROPOSAL.md Section 14 records the detailed proof/results.

Recommendation: review this late C++/encoded-object gate, then integrate a shared
storage-plan/audited-emitter service under a separately approved production step.
Analysis/LSP and AOT/ORC must share the exact target/profile, complete lowering
inventory and certificate delivery gate. Complete GC Frame/Slot/root/staging and
failure-publication inventories, other layouts/signatures and clone/merge lineage
remain necessary. Const-formal borrowing still requires alias, later-argument,
owner/root lifetime, safepoint and reload proofs; otherwise retain owned snapshots.
This scalar proof does not establish those contracts or a universal stack guarantee.
Stop for boss/wrench review. No commits, pushes, branches or remote modifications.

## P23-2 production audit integration Part 1: activation stopped, 14 October 2026

This checkpoint is not complete. The owner authorized a reusable service and first
production AOT integration, with an explicit stop on unproved storage/ABI/emission
boundaries. The attempted actual activation rejected existing managed CLI objects.
Two additional storage proofs are required. Production activation was withdrawn;
final emit_impl/emit_object/emit_runtime_object retain their original implementation.
There is no fallback from failed certification to ordinary emission. The staged
service is compiled and tested separately through compiler-level fixtures.

Staged changes:

- include/void/llvm_storage_audit.hpp and src/backend/llvm/storage_audit.cpp extract
  the LLVM 19.1.7 late MachineFunction and final MC mechanism into a reusable,
  owned, single-use AOT service. Its factory identity includes triple, CPU/features,
  DataLayout, PIC, actual code model, target-option/version identity and both
  optimization settings. The current production PassBuilder(no-TM) O2 behavior is
  preserved in the staged service and distinguished from the standalone probe's
  target-aware O2. Supported native stack-register families are x64 and AArch64.
- LoweringInventory requires per-function applicability, distinct original
  AllocaInst identities, purpose/category records and explicit completeness of all
  eight existing storage roles. Arguments, results, copies, roots, Frame/Slots,
  staging and native guards have separate purpose kinds. Actual allocation type,
  constant count and alignment supply target extents. Missing homes/categories,
  duplicate/foreign homes and missing functions are rejected. The shared lower()
  has an optional sink at its actual allocation sites; current public AOT and JIT
  leave that staged sink unused. Array arguments/results are not lowered natively.
- NativeStoragePlanner retains its unrounded total as well as its existing rounded
  total. The service uses this existing calculator for P, not a new estimator.
  The 65536-byte policy is unchanged. Allocation identity, not names or matching
  extents, controls R; potential lifetime coloring receives zero R. Original
  array-call graph closure and rejection of removed array calls/functions keep
  initially array-free callers from evading the proof when inline homes disappear.
- Objects remain private through the late audit, post-AsmPrinter comparison and
  MC SP/call verification. Success binds final bytes with SHA256 and delivers via
  a temporary file and rename. Failure preserves any existing destination.
  No production fault hooks or imported probe JSON were added. Unproved incoming
  ownership in policy-applicable functions remains a rejection, not an ABI guess.
- New arrays_storage_audit compiler suite covers normal/O2 acceptance, excess
  budget, incomplete categories, omitted homes, immutable/unique purposes, actual
  O2 inlining into an initially array-free wrapper, real reused storage with R=0,
  post-freeze unplanned homes, target mismatch, single-use delivery, unchanged
  array-free budget scope and the continuing source native-array rejection.
  Its synthetic root/Frame/Slot purpose fixture proves inventory plumbing only,
  not production GC walkers, owner lifetime, publication or readonly borrowing.

Actual activation evidence and stop:

The full Debug build reached normal managed-cli-main and managed-cli-handoffs
object generation through the switched public AOT path. Both failed certification
before delivery. build/p23-2-audit-activation-blocker.log contains the original
failure output; build/p23-2-audit-activation-blocker.json retains concise structured
facts for the failed functions and their original inventories.

1. main has original pointer and RuntimeFailure homes of 8/align8 and 272/align8,
   P=280. Both are conditional non-entry allocas. LLVM represents them with
   zero-sized, alignment-1 machine objects, F=40, while emitting real dynamic SP
   moves/adjustments. The physical dynamic homes are not represented by those
   zero-size records. F alone does not bound them, and zero-byte matching credit
   is unjustified. A target/CFG/lifetime proof or reviewed lowering change is
   required; no blanket dynamic-stack allowance or hoisting was implemented.
2. position.bump in handoffs has P=40: two 4-byte values, a zero-byte/align8 empty
   GC Slot array, and a 32-byte Frame. Its machine frame is 88 bytes and includes
   a 1-byte/align8 object for the original zero-byte Slot home. Exact extent
   matching fails. A zero-logical-extent/physical-object mapping with no unproved
   credit needs proof; neither logical sizes nor zero-length semantics changed.

Both confirmed shapes now have focused compiler-service negatives with retained
certificates at build/ci-{debug,release,sanitizers}/storage audit fixtures/
conditional-home-blocker.json and zero-slots-blocker.json. The service must reject
both and create no delivery object. The production activation was then removed,
not made permissive. All existing AOT objects were rebuilt through the restored
original emitter. Passing that restored path does not certify the staged service
for existing programs. ARRAYS_PROPOSAL.md Section 15 records the incomplete status,
actual evidence and next required proof, and corrects the outdated top-level status.

Actual local verification with the restored final path:

- Debug and Release complete builds pass. Existing 60 suites remain present and
  unchanged; adding arrays_storage_audit makes 61 suites. The focused Debug run
  of audit/foundation/runtime_generated/frontend/aot passed 5/5 before broadening.
- Release full matrix passes 61/61 (49.85 s), including all existing independently
  linked native AOT/CLI, source, runtime, reload, frontend, tooling and LSP suites.
- Debug full matrix finishes 60/61 (100.07 s). interfaces_source segfaults once
  after 2.79 s, with no actionable stack in CTest output. An isolated rerun passes
  (5.42 s), and a native GDB run exits normally with all interface checks passed.
  The cause is not established or attributed to baseline/current changes. This
  remains a validation limitation, not a clean Debug matrix or a diagnosed fix.
- Strict focused ASan/UBSan passes 3/3 (8.46 s): arrays_storage_audit (0.15 s),
  arrays_foundation (0.10 s), interfaces_source (8.21 s), with detect_leaks=1,
  abort_on_error=1, halt_on_error=1 and UBSan stack traces. No checks were disabled.
  The first sandbox attempt aborted all three because LeakSanitizer could not
  inspect processes. Repeating outside the sandbox with the same checks passed.
  GDB likewise initially could not ptrace in the sandbox; the native run above
  was outside it. No sanitizer finding or native crash trace was captured.
- Audit fixtures pass Debug (0.09 s) and Release (0.05 s) within the full runs,
  and retain both blocker certificates. Source native-array execution is still
  explicitly rejected. No Windows/macOS production run, hosted CI, six-build
  acceptance matrix or production ORC certificate integration is claimed.

Logs are build/p23-2-audit-debug-restored-build.log,
build/p23-2-audit-debug-all-tests.log, build/p23-2-audit-release-build.log,
build/p23-2-audit-release-all-tests.log, build/p23-2-audit-interface-repeat.log,
build/p23-2-audit-interface-gdb.log, build/p23-2-audit-san-build.log and
build/p23-2-audit-san-native-tests.log. The rejected trial activation is retained
separately, not overwritten by the restored build. NativeStoragePlanner's existing
foundation checks continue to pass; diff checks pass.

Stop for boss/wrench review. The next step is proving conditional and zero-extent
home reconciliation, or reviewing a semantics-preserving lowering change, before
switching public AOT delivery. Full inventories, general ABI ownership, exact
profile/cache hardening, clone/merge lineage, source diagnostic association,
GC/root/staging safety, readonly borrowing, reload and ORC/LSP remain open gates.
No source array lowering, budget increase, unrelated receiver-chain correction,
CI changes, commits, pushes, branches or remote modifications were made.

## P23-2 storage-shape proof, 14 October 2026

The two prior storage mismatches now have restricted, verified handling in the
staged service. Public AOT remains on the restored original optimizer/emitter
branch. No native arrays, budget change, ORC/LSP integration or activation occurred.
The earlier Debug interfaces_source segfault remains unexplained, not fixed or
attributed. ARRAYS_PROPOSAL.md Section 16 records the proof and remaining gates;
Section 15 is retained as historical failed-activation evidence.

Conditional homes:

- Verified pinned LLVM SelectionDAGBuilder::visitAlloca: absent a static entry-home
  mapping, a constant-size non-entry alloca uses DYNAMIC_STACKALLOC with target
  stack-alignment rounding. A cycle can execute it repeatedly. The prior F=40
  and zero-size frame objects therefore did not bound those physical homes.
- Added explicit compiler-owned cli_parse/cli_failure home kinds. Only these may
  move in the proof service. The producer supplies their existing main C signature
  and exact ABI types. The service checks role, shape, count one, allocation address
  space, ABI alignment, reachability, no cycle through the original block, and no
  stack save/restore or returns-twice control. Repeated/cyclic, variable-count,
  over-aligned, wrong-shape and stack-control fixtures fail closed.
- All validation completes before moves. Only the original AllocaInst moves to
  entry, preserving identity, type, count, alignment, users and debug information.
  Initializers and all other operations stay put. Tests inspect the initializer
  blocks directly. These hidden adapter homes are not VOID-addressable values or
  registered GC roots; the failure buffer remains initialized before invocation.
  Without stack restoration, each original allocation lived until function return
  and executed at most once. The narrowed change reserves static storage earlier
  without moving source evaluation, publication or failure behavior.
- The audit now rejects variable/opaque frames globally. Moved CLI homes also
  require SP writes confined to target prologue/epilogue regions. The emitted
  object must match the late machine snapshot. There is no dynamic-stack estimate,
  body-adjustment allowance or blanket acceptance of conditional allocas.
- Exact managed main/handoffs main frames have P=280, normal F=312 and O2 F=328;
  own F+E is 320/336 on native Linux x64, E=8. Outgoing space is already in F.
  Every CFG path, including the trace loop, uses this one static frame. Those
  array-free functions correctly retain C=null. The policy-applicable microfixture
  has P=280/F=280/R=280/E=8/C=288 in both modes. The 65536-byte policy is unchanged.

Zero-extent homes:

- Downloaded and independently inspected the exact LLVM 19.1.7
  SelectionDAG/FunctionLoweringInfo.cpp. Lines 143-159 explicitly compute the
  static alloca size, change zero to one physical byte, and pass the original
  AllocaInst to CreateStackObject. The source is retained under
  build/p23-2-shapes/FunctionLoweringInfo.cpp; the proposal links the pinned source.
- Acceptance requires exact original live/static provenance, still-zero current
  LLVM type/count, actual machine size one, sufficient alignment, a unique mapping
  and physical range wholly in F. The original zero logical extent and P are kept.
  R for this home is always zero; the measured physical byte/padding stays in F.
  Unknown zero clones and dynamic zero homes still fail. No logical extent,
  source value, zero-length semantics or GC root count is normalized to one.
- In exact position.bump, P remains 40 and normal/O2 F is 88/72. The empty Slot
  array IR and count-zero Frame call are unchanged. Runtime Frame validation and
  collection walk by count, so no Slot is read at count zero. Native managed
  cycle/handoff/failure comparisons and existing runtime/source suites exercise
  the preserved behavior. A direct Runtime regression additionally nests a
  zero-count Frame with null Slot storage above a live parent root, collects and
  verifies the root survives, then removes the parent and verifies collection
  releases the object. This is not a proof of future reference-array GC safety.

Shared producer and regressions:

emit_aot_storage_proof is an explicit compiler-fixture entry. It uses the actual
public AOT module/adapter producer and its optional inventory sink, then selects
the staged service. Public emit_object/emit_runtime_object never select this entry;
they retain the original emission branch and conditional allocation locations.
No permissive fallback, CLI activation flag or separate IR producer was introduced.
Original, entry-normalized and optimized IR plus final certificates are retained.

The expanded arrays_storage_audit suite now covers the real
examples/managed_structs/main.void and handoffs.void modules, normal/O2, along with
positive zero-size and CLI-home proofs. Replaying their exact original IR without
the CLI permission rejects while preserving an existing destination object.
Missing inventories, excess budget, untracked/inlined homes, real reused storage,
source-native-array rejection and the original assertions remain. Added negatives
also reject cyclic CLI allocation, variable counts, unsupported alignment/type,
stack control, untracked zero clones and dynamic zero-size allocations. No false
failure was converted to a pass by reducing coverage or optimization.

Corrected the earlier Section 15/work-log shorthand examples/managed/main.void to
the actual managed_structs directory. The failing baseline evidence is unchanged.

Interface-crash investigation:

- First used the unchanged Debug binary for six serial and eight two-worker
  executions: 14/14 passed. Results/logs are under
  build/p23-2-shapes/interfaces-original. No original crash was reproduced.
- Added flushed phase and normal/O2 progress around the existing interface test
  bodies. No assertion, operation, timeout or optimization inside those tests was
  removed. The new tools/ci/interfaces_probe.py isolates each child working/core
  directory, requests a bounded core size and records every failure or timeout.
- Eight serial and 24 four-worker runs with progress passed 32/32. Six additional
  native GDB runs with ASLR explicitly enabled also exited normally. The GDB script
  would retain all-thread full backtraces and a core on an abnormal stop. No signal,
  crash backtrace or core was captured. Final runner hardening sets inherited
  core limits before starting threads, avoiding Python preexec callbacks in a
  threaded parent. Two serial/eight four-worker final runs also pass 10/10.
  These 62 controlled passes do not establish a fix, baseline attribution or
  irrelevance to the storage work.
- Current full Debug/Release matrices and the strict interface sanitizer run pass.
  The historical isolated segfault remains a review limitation. Progress/core
  evidence should be preserved if it recurs; a successful retry must not erase it.

Actual validation:

- Full Debug build and 61/61 tests pass (100.93 s). Full Release build and 61/61
  tests pass (51.92 s). This preserves all original 60 suites plus the staged
  compiler audit suite. Production AOT activation remains disabled in both runs.
- Strict ASan/UBSan passes audit/foundation/runtime_generated/interfaces_source,
  4/4 (10.02 s), with leak detection, abort/halt on error and UBSan stack traces.
  This ran outside the known ptrace-restricted sandbox; no checks were disabled.
- After adding the final exact-source fail-closed replay cases
  and the direct zero-Slot collector case, the final audit suite passes Debug
  (0.75 s), Release (0.45 s) and strict sanitizers (1.07 s). These extensions changed
  tests only; shared product code was unchanged from the full-matrix/native runs.
- tools/ci/storage_shape_probe.py independently links public-original and
  staged-certified objects against the same runtime for main, handoffs and
  null_failure, normal/O2. Each build compares eight argument cases at inherited
  and 1 MiB hard/soft RLIMIT_STACK. Debug and Release each pass 96 comparisons,
  192 native process executions. Total: 192 matching exit/stdout/stderr comparisons,
  384 executions; 192 executions use 1 MiB. Source failure, malformed CLI input,
  i32 range errors and valid input agree exactly. No failed object is executed.
- Machine certificates for every staged native case reject dynamic/opaque shapes;
  exact main/handoff zero-home mappings and final object disassembly are retained.
  Native/public objects remain independent, not LTO-merged. No artificial noinline,
  volatile suppression, stack-reserve increase or guessed overhead was used.
- Python compilation, whitespace and diff checks pass. No hosted or native
  Windows/macOS verification, six-build acceptance matrix or full sanitizer matrix
  is claimed for this checkpoint.

Reproduction: tools/ci/STORAGE_SHAPE_PROOF.md, tools/ci/storage_shape_probe.py and
tools/ci/interfaces_probe.py. Evidence: build/p23-2-shapes/native-debug-final and
native-release contain complete.json, commands.json, results, IR, certificates,
disassembly and binaries. The full/focused build/test logs and controlled interface
runs are under build/p23-2-shapes. Compiler-level certificates are under
build/ci-{debug,release,sanitizers}/storage shape fixtures, including exact unmarked
rejections. interface-gdb-repeats.log and interface-repeat.gdb retain native GDB
procedure/results. The LLVM source fetch initially needed network access outside
the sandbox. An initial native driver link failed because resolving the clang++
symlink changed its driver mode to clang; preserving the supplied C++ driver name
corrected the harness. This was not a compiler/runtime failure and no production
code was altered for it. Only complete verified native directories are cited above.

Stop for review. The two identified shapes are proved only for the narrowed
staged paths above. Public AOT activation, certification of the entire AOT corpus,
new-shape native platform coverage, general ABI ownership, source/profile/cache
hardening, optimization lineage, full GC/root/staging and borrow/reload proofs
remain open. The unexplained Debug interface crash also remains unresolved.
No source native arrays, ORC/LSP work, unrelated receiver-chain correction, CI or
Git mutations occurred. No commits, pushes, branches or remote modifications.

## 15 October 2026: P23-2 production AOT certification Part 2

Owner reports CI #41 passed all seven jobs, 61/61 suites/configuration. Section 16
is locked. Reviewed that section and the preceding work-log evidence before this
checkpoint. Public AOT remains on its restored original branch. No source native
arrays, ORC/LSP activation, budget change, CI changes or Git mutations.

Implemented only staged integration/proof hardening:

- Added explicit source/architecture/runtime adapter selection to the investigation
  entry, reusing emit_impl and the actual original generation suffix. Seven family
  library fixtures, three edited reload producers and generated-runtime IR accept
  an explicit test-only --storage-proof argument. Existing arguments still select
  their original public path. The audit test target also links the existing
  examples/proof.cpp module producer; no new suite or alternate builder was added.
- The same owned LLVM 19.1.7 TargetMachine/profile supplies freeze and emission.
  Function CPU/tuning/features/ABI attributes must agree. Frozen normalized IR,
  immutable target-layout inventory serialization and optimized IR now have SHA-256
  identities retained alongside the emitted object hash. Edits between freeze,
  optimization and delivery reject. Tests cover profile/content mismatches and
  show that mutating the caller's inventory cannot mutate the frozen service.
- Every remaining definition must have PEI, late and final-emission coverage.
  Available-externally/no-machine-code coverage now has an explicit negative.
  Unknown non-spill homes and unproved incoming ownership reject array-free
  candidates as well as array-policy candidates. No credit is awarded to unknown
  homes. Reports preserve their exact object index, extent/alignment, current
  alloca text or absence of an identity. Dynamic/opaque and final SP/call checks
  remain mandatory. Existing reuse fixture still proves P=512, R=0; array-free
  and array-policy inline clones reject. The 65,536-byte constant is untouched.
- Corpus exploration found a staged reporting lifetime bug. entry_home_moves
  retained a non-owning block-name StringRef across optimization. The first
  incomplete attempt exposed invalid UTF-8 bytes in original_block after O2
  removed the block. Freeze now copies that name into owned storage. A regression
  checks the original parse/run names after normal/O2, and the final corpus uses
  strict UTF-8 decoding of every log/report. No source diagnostic, compiler ABI,
  runtime or public-emission semantics changed. This does not establish a cause
  for the earlier interfaces_source crash.

Complete production corpus investigation:

- tools/ci/aot_certificate_corpus.py reads CTest's configured integration commands
  and CMake's seven source families. All 15 AOT-related scripts pass their original
  assertions through an output/status-preserving capture wrapper. Every successful
  emission is attempted separately at the exact current source snapshot, before
  imported/generated files change again. Architecture and edited-source fixtures
  are additionally attempted in normal mode; their ordinary scripts emit O2 only.
- 334 staged candidates, 167 per profile: 247 accepted, 87 rejected. Integration
  source/architecture: 208 attempted, 200 accepted. Family source CLI: 104/42.
  Runtime libraries: 14/3. Edited reloads: 6/1. Generated runtime IR: 2/1.
  Normal/O2 breakdown and exact routes are in proposal Section 17 and records.jsonl.
  Every rejected candidate preserves an existing sentinel object. None is linked,
  run or replaced by fallback emission.
- All 242 accepted CLI objects are independently linked against the same runtime
  and compared with ordinary controls: 1,452 identical exit/stdout/stderr cases,
  2,904 native executions. Four complete native harness pairs pass all original
  assertions: managed, managed_initialization, inheritance and runtime, normal.
  The other 12 library configurations are explicitly rejected/not linked when
  either required generation fails. A partial library pair is not a native pass.
- Reports cover 1,972 machine functions and 32 observed combinations of home kinds,
  CLI moves, zero extents, lifetime markers, incoming and unmatched objects. Full
  per-object extents/offsets/identities and final encoding observations are retained.
  Observed kinds: value, argument, root, Frame, Slot array, staging, CLI parse and
  CLI failure. There are 310 function-level CLI move observations, 29 zero-home,
  40 lifetime-marker, 14 incoming and 309 unmatched observations. The unmatched
  objects span 24 size/alignment/alloca-presence combinations. No corpus frame is
  dynamic/opaque after the narrowed CLI normalization; focused negatives still
  reject such shapes. 575 functions change after PEI. 395 have local objects below
  the measured local frame, including ordinary Linux red-zone storage. Those
  array-free C=null reports do not prove an array budget or general ABI bound.

Confirmed activation blockers, left fail-closed:

- Raw aggregate ABI incoming ownership is unproved in existing value-struct paths.
  Normal choose has P=49, F=0, with 8-byte and 1-byte incoming objects at entry
  SP+16/+8. The numeric fixture has further incoming stack objects. Normal numeric
  entry also has an unmatched 64-byte/align-8 machine object with no AllocaInst
  identity (index 47, P=480, F=600). No general ABI credit or allowance was guessed.
- O2 leaves unmatched transformed storage in managed functions and receiver/main
  wrappers. Matching extents/names or a current AllocaInst do not prove original
  lineage. Under the stronger coverage contract, the previous O2 managed positives
  become explicit destination-preserving rejection tests. Normal exact managed
  positives remain, and scalar source CLI positive tests cover both profiles.
- Numeric normal final encoding changes PUSH64i32:i1 to PUSH64i8:i1; its remaining
  recorded SP operands agree. This is outside the existing proved normalizer and
  remains rejected. Three O2 inheritance stress CLI/library/reload thunks also
  disagree on call classification: late TAILJMPd64 has no MC call marker, while
  four CALL64pcrel32 entries remain. SP lists agree for that thunk. No tail-pseudo
  or signed-push equivalence was silently admitted.
- Rejection categories overlap: unmatched objects in 82 candidates, incoming
  ownership in 14, final SP mismatches in two, final call mismatches in three.
  Focused actual value-struct and inheritance tests retain the evidence and assert
  existing outputs survive. Exact native ABI/encoding expectations are Linux x64
  only; other targets retain portable service/scalar cases without invented results.

Actual local validation:

- Complete Debug build and 61/61 tests pass, 65.83 s with four CTest workers.
  This includes ordinary native AOT, compiler/frontend/IR/runtime/GC/reload,
  string/interface and editor/LSP tests. Public activation was off throughout.
- Release audit suite passes. After the final actual aggregate/tail-call rejection
  regressions, Debug audit passes 1/1 (1.39 s), Release 1/1 (0.78 s).
- Strict ASan/UBSan audit/foundation/runtime_generated/interfaces_source passes
  4/4 (10.11 s). The final extended audit passes again, 1/1 (1.72 s). Leak detection,
  abort/halt on error and UBSan stack traces stay enabled. These native runs used
  approved execution outside the ptrace-restricted sandbox; no check was disabled.
- Python compilation and whitespace/diff checks pass. Final native corpus
  complete.json is true, with the separate 247 accepted/87 rejected counts above.
  The initial incomplete corpus directory is preserved as reporting-bug evidence,
  not cited as a successful experiment. Later test-only additions/target guards
  received the final focused checks; shared product code stayed unchanged after
  the full Debug and complete corpus runs.

Reproduction: tools/ci/STORAGE_SHAPE_PROOF.md. Evidence:
build/p23-2-part2-corpus-final contains complete.json, records.jsonl,
shape-inventory.json, all 334 certificates and three IR snapshots per candidate,
objects/sentinels, native binaries and harness logs. Build/test logs use the
build/p23-2-part2-* names. No new 1 MiB test, full Release/sanitizer/six-configuration
matrix, hosted run, Windows/macOS native execution or cross-target validation is
claimed. Historical small-stack observations do not validate newly rejected O2
objects. The older storage_shape_probe.py is documented as a historical Section 16
driver, not an all-pass gate for the stricter service.

Stop for boss/wrench review. General incoming ABI ownership, original-to-transformed
storage lineage, allocationless machine homes, exact push/tail encoding proof,
general red-zone ownership and target-native coverage remain unproved. The next
checkpoint should address those concrete boundaries before activation, preserving
current rejection. Source native arrays, GC/root/staging completeness, const-formal
borrow/reload, ORC/LSP and receiver-chain hardening retain their separate gates.
The historical Debug interface crash remains unexplained; existing phase/mode/core
capture instrumentation is preserved and passing tests do not establish a fix or
attribution. No commits, pushes, branches, remote modifications or CI edits.

## 15 October 2026: P23-2 production AOT certification Part 3

Owner reports CI #42 passed all seven jobs, 61/61 suites/configuration. Part 2 is
locked. Reviewed proposal Section 17 and its retained evidence. Workspace and files
were present; the initial working tree was clean. No public AOT, source native-array,
ORC/LSP, ordinary ABI, 65,536-byte policy or CI activation change was made.

Incoming and ABI-created storage investigation:

- Downloaded pinned LLVM 19.1.7 call/frame lowering, return demotion, InstCombine,
  SROA, inlining and stack-coloring sources for the actual implementation rules.
  The first sandbox fetch failed DNS and was retried with approved network access.
  Sources and SHA256SUMS are retained in build/p23-2-part3-sources, with exact pinned
  links in proposal Section 18. No LLVM source was modified or substituted.
- Added tools/ci/aot_incoming_probe.py. It requires native Linux x64, pinned tools,
  GDB and ptrace permission, and executes only the distinct original production
  AOT controls. GDB retains entry/call/callee-entry/return/cleanup SP, verifies
  argument bytes at identical addresses, actual incoming alignment, control-file
  hashes and normal exits. No failed candidate is executed or delivered.
- For original normal choose, caller F=280, and the caller pushes another 16 bytes
  below its static frame. Callee incoming objects are 1 byte at entry SP+8,
  alignment 16, and 8 bytes at SP+16, alignment 8. The logical nine-byte extent sum
  is not its physical caller argument area. For exchange, caller F=600 and the
  additional outgoing area is 144 bytes, with 17 incoming objects at +8..+136.
  MFI reports maxCallFrameSize=136. The native 144-byte use includes real alignment
  padding. CALL adds eight return-address bytes; RET restores call-site SP and
  caller cleanup restores entry SP minus F. No generic ABI credit follows from an
  incoming offset or the existence of a fixed machine object.
- The pinned x86 target excludes push sequences from reserved call frames. Thus
  hasVarSizedObjects=false does not prove all outgoing storage is inside F. The
  reusable service now records reserved-call/tail state and FinalizeISel frame
  objects. Policy-applicable nonreserved/nonzero outgoing areas fail closed,
  without adding a guessed area or rounding allowance. A normal/O2 negative has
  P=65,520, F=65,528, R=65,520, E=8 and nominal C=65,536 but a nonreserved 40-byte
  area. It rejects at the new guard; the existing PUSH rejection is also retained.
- Independent pinned MIR identifies numeric normal's 64-byte/align-8 FI47 as the
  hidden result home: LEA to RDI at exchange, followed by returned-field loads.
  Native original control verifies RDI equals caller entry SP-112 (FI offset -120
  plus E=8) and exchange writes expected result fields. Return-demotion source
  creates this object without AllocaInst identity. This establishes its role in
  the fixture, not a general immutable result-home mapping. FI47 stays rejected.
- No incoming ownership is newly marked proved. General tail/byval/copy-elision/
  variadic mappings, full outgoing CFG/SP bounds, red-zone ownership and target
  differences remain unproved. No new Windows/macOS native or cross-target claim.

Narrow lineage extension, with conservative charging:

- The actual production PassBuilder(no-TM) O2 pipeline now has read-only storage
  observations. Weak handles distinguish deletion/address reuse. Reports record
  births, deaths and descriptor changes after actual pass boundaries, including
  SROA, InstCombine, inlining and alignment observations. These are evidence, not
  inferred partition or clone mappings. No optimization is suppressed or replaced.
- Managed main/handoffs mismatches include InstCombine replacing alloca i8,N with
  alloca [N x i8],1. A live CallbackVH observes that exact RAUW edge while both
  objects exist. The rule is restricted to original Frame/roots or staging/staging
  identities, the actual InstCombine pass, same-function static/default-address-
  space homes, exact positive byte count/type, unchanged alignment and no special
  inalloca/swifterror form. A replacement claimed by another original home rejects.
  Metadata, names, after-pass shape similarity and copied clone names grant nothing.
- Original P stays immutable. Canonical homes match their exact late machine
  allocation and extent, but receive ZERO R credit. Every physical byte remains
  in F. The 40,000-byte canonical negative rejects at C=80,016; this conservative
  overcharge is not a new allocation limit or permission to increase the budget.
  Future large staging still needs stronger proof before acceptance.
- Added frame/staging positives, missing/altered role/type identity negatives,
  over-budget rejection, two-original/one-physical canonical reuse and near-limit
  nonreserved-call negatives. Canonical reuse has P=64, F=40, R=0, C=112; both exact
  original rewrite edges remain distinct. The original P=512/R=0 reuse assertion
  remains. Existing inline, zero-home, inventory/profile/module/coverage and source
  native-array rejection assertions remain intact.
- Former real optimized managed candidates now certify, and replay of their same
  normalized module without compiler home identities retains the original unknown-
  allocation/destination-preservation assertions. Callback recovery is not applied
  to inline clones, inferred SROA partitions, unrelated replacements or ABI temps.
  The separate PUSH/tail normalization logic is unchanged.

Complete corpus and native reruns:

- All 15 original AOT integration scripts pass. Full staged corpus: 334 attempts,
  275 accepted / 59 rejected, versus 247/87 in Part 2. Exactly 28 O2 candidates gain
  acceptance; no normal outcome changes. Integration attempts accept 100/104 per
  profile. Family CLI accepts 41/52 normal and 25/52 O2. Libraries accept 3/7 and
  2/7; reloads 1/3 and 1/3; generated runtime 1/1 and 1/1.
- All 288 stable-source routes matching Part 2 have identical profile, frozen IR,
  optimized IR, immutable inventory and private object hashes. Generated integration
  directories have deliberately different physical source identities and are not
  compared across runs. Accepted on-disk object/IR/inventory hashes are also checked
  independently by the corpus driver. No source, ABI or emitted instruction change
  accounts for the new acceptances.
- 266 accepted CLI candidates pass 1,596 exact exit/stdout/stderr comparisons,
  3,192 native executions. Seven complete native assertion harness pairs pass:
  managed normal/O2, managed_initialization normal, inheritance normal, strings O2
  and runtime normal/O2. Nine configurations remain explicitly rejected/not linked.
  Every failed candidate preserves its sentinel; none is linked or executed.
- Across 1,972 observed functions: 454 proved count-rewrite edges, 447 canonical
  machine homes with zero credit, 200 remaining unknown homes in 80 functions,
  including 51 without AllocaInst identity. Incoming ownership remains unknown in
  14 functions. Eleven function observations have nonreserved/nonzero outgoing
  areas; these corpus functions are array-free C=null, not array-budget verdicts.
  Rejection reasons overlap: 54 modules unknown storage, 14 incoming ownership,
  two final SP mismatches and three final call mismatches. Encoding counts match
  Part 2; no unfamiliar opcode is normalized.
- The historical three-program small-stack driver stops on optimized null_failure's
  remaining inline homes, before linking/execution. That incomplete directory is
  preserved. Added an explicit --programs selector with defaults unchanged and
  failed-selected-candidate rejection intact. A fresh main/handoffs normal/O2 run
  passes 64 comparisons, 128 executions, including 64 with hard/soft 1 MiB stack
  limits. The complete corpus still includes and rejects null_failure O2.

Local validation and tooling limits:

- Full Debug build/tests pass 61/61 (69.56 s, four workers), including the extended
  audit (9.04 s). Full Release build/tests also pass 61/61 (30.16 s, four workers).
  Release focused audit passes 1/1 (2.36 s).
- Strict ASan/UBSan audit/foundation/runtime_generated/interfaces_source pass 4/4
  (16.56 s), with leak detection, abort/halt on error and UBSan stack traces enabled.
  This uses approved execution outside the ptrace-restricted sandbox, without
  disabling checks. The audit alone takes 6.58 s; all new identity/reuse/budget and
  actual production-path regressions are included.
- One sanitizer compilation overlapped a fixture-file edit and ended with invalid
  byte diagnostics and Clang SIGBUS. The complete log, preprocessed source and
  generated compiler reproduction script are preserved in
  build/p23-2-part3-toolchain-crash. A stable-file rebuild and strict tests pass.
  The timing is recorded; no formal compiler-crash cause or fix is claimed.
- Python compilation and whitespace/diff checks pass. Native GDB witnesses verify
  clean control exits; their final verified run uses the new complete corpus.
  No hosted run, full six-configuration acceptance or non-Linux native result is
  claimed. Interface crash progress/core instrumentation is untouched; existing
  successful Debug/sanitizer runs do not establish a cause or fix for the prior
  unexplained interfaces_source failure.

Evidence: build/p23-2-part3-corpus/{complete.json,records.jsonl,
part3-evidence-summary.json}, all certificates/IR/objects/native harness logs;
build/p23-2-part3-native-witness-verified, numeric MIR in p23-2-part3-native,
pinned sources/SHA256SUMS, and p23-2-part3-small-stack-certified. All build/test
logs use build/p23-2-part3-* names. Proposal Section 18 and
tools/ci/STORAGE_SHAPE_PROOF.md record tracked reproduction and the proof boundary.

Stop for boss/wrench review. Full incoming ownership/charging, original-to-inline/
partition lineage, codegen-origin identity for ABI results, complete target-native
proofs and the separate PUSH/tail checks remain blocking. A trustworthy origin hook
or separately approved producer transformation is needed; matching shapes/FIs are
insufficient. No guessed ABI allowance, larger budget, LLVM fork, source array
execution, ORC/LSP integration, unrelated receiver-chain work, commits, pushes,
branches, remote modifications or CI edits.

## 15 October 2026: P23-2 production AOT certification Part 4

Owner reports hosted CI #43 passed all seven jobs, 61/61 suites, locking Part 3.
Reviewed proposal Sections 17–18 and the latest work-log evidence. Investigation
stops at a confirmed final encoded-CFG/call-site ownership proof boundary. Public
AOT remains on its original path; source native arrays remain disabled; the
65,536-byte budget and separate lineage, incoming, PUSH/tail gates are unchanged.
Proposal Section 19 records the findings and the next review decision.

### Outgoing storage findings and scoped changes

- Rechecked all eight pinned LLVM 19.1.7 source checksums. Reserved areas are already
  in F. X86 push conversion can instead place arguments below F, and call-frame
  lowering rounds nonreserved areas, factors pushes/callee cleanup and merges SP
  updates. One stack argument can stay reserved due to the actual profitability
  rule; five stack arguments occupy 48 bytes despite raw area 40. No global
  maxCallFrameSize addition or guessed allowance is safe.
- Added diagnostic-only outgoing_sp_observation to the existing auditor. It records
  entry-relative SP depth across machine blocks, successors, exact supported
  arithmetic/push/pop events and direct C call depths. Joins/backedges must balance;
  unknown paths/calls/SP operations, dynamic allocation, tails, realignment and
  uncovered blocks get a null observation. It gives zero incoming/reconciliation
  credit and explicitly does not bind the final encoded CFG. No acceptance rule,
  immutable P, R rule, target profile, LLVM pipeline or public delivery path changes.
- A complete late-machine observation suggests O=max(0,peak_depth-F), avoiding
  double-counting reserved areas or summing alternative/loop iterations. This is
  not yet an adopted charging equation. The existing nonreserved/nonzero-area
  rejection stays in place, including below-budget observations.
- Added normal/O2 compiler fixtures for reserved/padded areas, multiple calls,
  branching, balanced repeated calls, 65,456/65,520-byte homes, hidden results,
  tail calls and unsupported dynamic/indirect/realigned forms. Owner-destination
  preservation and all previous assertions remain. The new report owns diagnostic
  error text, including temporary opcode messages; strict JSON/UTF-8 readers pass.
- Added tracked aot_outgoing_probe.py. Independent ordinary LLVM controls link a
  separate C caller/callee assertion harness. GDB steps over callees and checks
  actual caller SP, call alignment, return/cleanup, results and visit counts.
  Normal/O2 observations and native maxima agree: padded O=48; both branch paths
  and all three loop iterations are exercised. The two near-limit nominal charges
  are 65,472/65,536; including measured O would yield 65,520/65,584. Both still reject
  pending proof. Hidden-result buffers retain unmatched-origin rejection.

### Confirmed architectural stop

The final object checker currently binds flattened SP-writer and separate call
lists, not branch targets, block extents, event interleaving or path multiplicity.
A bytes-only negative retargets a real control's conditional branch to its frame
setup while leaving decoded SP/call lists unchanged. That altered file is never
executed or delivered. This demonstrates missing certificate information; it does
not claim the compiler emitted a bad branch or accepted an altered object.

Early's per-callee maximum pseudo area also loses individual call-site assignment,
cleanup and origin information. Matching a name, size or FI is not an ownership
proof. Acceptance widening stops here. Recommend architectural review of final MC
CFG/event binding or an LLVM-origin emission hook, plus persistent per-call ABI/
result-origin evidence before pseudos disappear. No LLVM fork/hook or architectural
choice is silently introduced. Incoming ownership, hidden-return origin and general
optimization lineage remain unproved; signed PUSH/tail normalization stays separate.

### Actual local verification

- Full Debug 61/61, 75.24 s (audit 8.81 s); full Release 61/61, 37.54 s
  (audit 3.04 s). No test suite or safety assertion was removed.
- Strict ASan/UBSan audit/foundation/generated-runtime/interfaces 4/4, 15.23 s
  (audit 6.45 s), leak detection, abort/halt and stack reporting enabled outside
  the ptrace restriction. No full sanitizer matrix is claimed. The initial build
  command used a nonexistent generated-runtime target name; the corrected target
  build and this final test run pass.
- Complete staged corpus: 334 attempts, 275 accepted / 59 rejected, all individual
  outcomes unchanged from Part 3. The 288 exact source-identity routes retain
  identical profile, frozen/optimized IR, inventory and private-object hashes.
  All 15 original integration scripts pass. Accepted CLI comparisons: 1,596,
  3,192 native executions. Seven complete assertion harness pairs pass (14 runs);
  nine configurations remain rejected/not linked. Sentinels survive all rejections;
  no rejected candidate is executed. Rejection counts remain 54 unmatched, 14
  incoming, two final-SP and three final-call, overlapping by module.
- 1,972 machine functions: 1,670 complete diagnostic CFG observations, 302 null.
  All 11 nonreserved/nonzero-area observations remain array-free C=null and earn
  zero credit. New controlled tail/dynamic/indirect/realigned negatives have null
  bounds and still reject. This does not constitute complete outgoing certification.
- New outgoing witnesses: 36 GDB executions plus 72 ordinary native controls,
  36 with hard/soft 1 MiB stack. The first sandboxed GDB attempt failed with ptrace
  Operation not permitted; the final permitted run passes with ASLR enabled.
  Bytes-only branch negative is never run.
- Real original production main/numeric GDB witnesses pass again: F/peak/O are
  280/296/16 and 600/744/144, with argument bytes/addresses, alignment, hidden-result
  pointer/fields, call return, cleanup and normal-exit assertions preserved.
- Production managed main/handoffs normal/O2 small-stack selections pass in Debug
  and Release, each 64 comparisons / 128 native runs, including 64 at 1 MiB. The
  full corpus retains rejected null_failure; it is not claimed as selected success.
- Historical Debug interfaces_source crash instrumentation is unchanged. No crash
  recurrence, attribution or fix is claimed from passing runs. No Windows/macOS
  native/cross execution, hosted acceptance, ORC/LSP certification or native-array
  execution is claimed.

Artifacts: build/p23-2-part4-corpus/{complete.json,records.jsonl,
part4-evidence-summary.json}; build/p23-2-part4-outgoing-final;
build/p23-2-part4-production-native; build/p23-2-part4-small-stack-{debug,release};
and build/p23-2-part4 build/CTest logs. Reproduction instructions are in
tools/ci/STORAGE_SHAPE_PROOF.md. Stop for boss/wrench review. No Git mutations,
remote modifications, CI changes or unrelated compiler/runtime edits.

## 15 October 2026: P23-2 Part 5A encoded-object CFG research pilot

Owner reports CI #44 is locked. Reviewed the Research Clanker hybrid architecture
report supplied in this conversation, proposal Section 19 and the rolling work log.
Implemented only the Linux x64 independent encoded-CFG checkpoint. The report's
emission-origin, ABI-ledger and reconciliation stages remain deferred. Proposal
Section 20 and tools/ci/STORAGE_SHAPE_PROOF.md describe scope and reproduction.

### Implemented and confirmed

- Added read-only AotObjectEmitter::research_encoded_cfg and a small internal
  encoded_cfg.cpp service using the existing exact TargetMachine/profile, LLVM
  19.1.7 ObjectFile/ELF, MCDisassembler, MCInstrInfo and MCInstrAnalysis facilities.
  The explicit --research-cfg option exists only in the compiler fixture binary.
  Public AOT, deliver/checkObject decisions and the 65,536-byte policy are unchanged.
- Reconstructs exact instruction intervals, local direct branches/fallthroughs,
  direct-call continuations, ordered SP/call/return events and reachable exact SP
  depths. Unknown opcodes/SP writes, indirect flow, unfamiliar relocations/returns,
  targets outside a function or into an instruction, missing fallthrough, inconsistent
  joins/growing cycles and incomplete byte coverage reject. Supported call relocation
  semantics are restricted to PLT32/PC32 displacement fields with addend -4; linked
  calls need an exact function-symbol target. Unknown thunks/branch relocations reject.
- Added layout/ordered_events to the existing machine observation while preserving
  its old sp_events data. Independently decoded event order/edges/depth/peak compare
  against complete same-profile machine evidence. No arbitrary graph matching,
  origin guess, emission labels or new subtraction credit is introduced.
- Explicitly selected-function research scope, finite opcode subset, ELF ABI checks
  and analysis workload caps are documented. Research guards (1 MiB symbol extent,
  4,096 instructions/blocks/events) are not compiler storage limits. Results always
  deny delivery, ABI ownership and physical-memory-bound credit. Ordinary Linux x64
  C call-return SP behavior is a stated pilot contract, not an implemented ABI ledger.
  Red-zone/stack-derived memory, probing, full linker coverage and recursive-chain
  storage remain separate obligations.
- Mandatory exact Part 4 original passes (peak 88) and its untouched, never-executed
  retarget rejects as inconsistent join/growing cycle, both with and without machine
  evidence. Independent disassembly confirms identical flattened SP/call lists.
  Retained original SHA-256 remains E56F290C3DC2902D01099A043F02F78D272487BDF03C2B9BB0E29061318F58A0.
- Added self-contained normal/O2 compiler regressions for an emitted two-path function
  and copy-only cycle/mid-instruction mutations. Existing assertions, sentinel tests,
  native-array prohibition, signed-PUSH/tail, lineage and ownership gates remain.
- Added tracked aot_encoded_cfg_probe.py. It tests linked/relocatable normal/O2 cases,
  both alternatives, balanced loops, padding, multiple calls, near-limit homes and
  hidden results, plus malformed instruction/edge/relocation/machine-evidence cases.
  Malformed LLVM JSON errors are consumed before rejection; foreign ABI flags and
  missing machine fields reject. CLI report/input alias guards preserve evidence.
  Collision tests use disposable copies, never risk the retained originals. Wide/
  narrow signed pushes compare stack effects only; the production encoding gate
  remains unchanged. All negatives are never executed.

### Actual verification

- Final encoded-CFG driver: 92 checks in each of Debug, Release and strict ASan/UBSan,
  each 60 supported research cases / 32 required negative or input-preservation
  checks. Includes the exact retained mandatory pair, unchanged input hashes,
  machine-evidence alterations, malformed JSON, ELF profiles, instruction-resource
  boundary 4,096 pass / 4,097 reject, and report collision protection. No input is
  modified or executed by this read-only driver.
- Full Debug 61/61, 90.96 s (audit 10.58 s); full Release 61/61, 33.20 s
  (audit 2.82 s). Final research-input guard changes were followed by focused
  audits: Debug 9.03 s, Release 2.65 s, and all final 92-case drivers.
- Strict ASan/UBSan audit/foundation/generated-runtime/interfaces 4/4, 16.44 s
  (audit 6.49 s), leak detection/abort and halt/stack reporting enabled with permitted
  ptrace. The independent 92-case sanitizer driver retains the same strict settings.
  No full sanitizer/six-configuration matrix or new hosted result is claimed.
- Complete staged corpus: 334 attempts, 275 accepted / 59 rejected, every outcome
  unchanged from Part 4. All 288 exact source-identity routes retain identical profile,
  frozen/optimized IR, inventory and object hashes. All 15 original integration
  scripts pass; accepted CLI objects pass 1,596 comparisons / 3,192 native executions.
  Seven complete assertion harness pairs pass (14 runs); nine remain rejected/not
  linked. Rejection counts stay 54 unmatched, 14 incoming, two SP/PUSH, three call/tail,
  overlapping by module. All rejection sentinels survive; no rejected object executes.
- Native outgoing witnesses pass again: 36 GDB runs plus 72 ordinary runs, 36 at
  hard/soft 1 MiB with ASLR enabled. Debug/Release production main/handoffs selections
  each pass 64 comparisons / 128 runs, including 64 at 1 MiB. Rejected null_failure
  stays in the complete corpus rather than being claimed as selected success.
- Debug interface crash capture remains untouched. Passing tests establish no
  attribution or fix for the historical crash. No Windows/macOS execution, complete
  physical storage certificate, source native arrays or ORC/LSP activation is claimed.

Artifacts: build/p23-2-part5a-cfg-{debug,release,sanitizers}-complete;
build/p23-2-part5a-corpus/{complete.json,records.jsonl,part5a-evidence-summary.json};
build/p23-2-part5a-outgoing-native; build/p23-2-part5a-small-stack-{debug,release};
and matching build/CTest logs. Exploratory runs are retained separately.

Stop for boss/wrench review. The bounded Part 5A milestone is met. Emission-origin
hooks, exact attribution/ABI ownership, memory-region proof, storage reconciliation
and production publication remain future gates. No commits, pushes, branches,
remote changes, normal CI edits or unrelated runtime/compiler semantics changes.

## 15 October 2026: superseding owner correction, large-array size limits

Owner confirms CI #45 green, 7/7 jobs and 61/61 suites per configuration; Part 5A
is locked. The owner explicitly supersedes the earlier 65,536-byte production
per-function array-storage approval. This entry records a documentation-only
correction, not a compiler change or approval of an alternative representation.

### Documentation changes

- ARRAYS_PROPOSAL.md now opens with the superseding decision and replaces A10's
  production byte/expanded-array ceiling language. A1/A2 express value/snapshot
  semantics independently of physical placement; the original A8 inline diagram
  is labeled a candidate rather than a universal stack requirement. Sections 8-20
  retain original approvals, numerical measurements and failures as historical
  research evidence, explicitly superseded for production policy. Section 21 gives
  the unchanged-code audit and activation gates.
- DECISIONS.md records that neither `[T, n]` nor `[T]` may have an arbitrary 64 KiB
  source-level ceiling, and that no replacement number, representation, stack/heap
  cutoff or production reactivation is approved.
- LANGUAGE_BIBLE.md Section 32 records the agreed large-array commitment with
  implementation pending. Large fixed values preserve independent snapshots and
  existing element/reference/readonly semantics. Inline storage is not a requirement
  that every payload live on the native stack. Future string indexing stays separate.
- DEVELOPMENT_STRATEGY.md adds the revised review/activation sequence. KNOWN_ISSUES.md
  documents current guards as limitations, including old expanded-field descriptions,
  rather than presenting them as permanent array semantics. All documents require
  complete builds: unsupported/failed required functions diagnose and fail the whole
  build; no silently omitted function or partial-success delivery.

### Read-only code audit and implementation impact

The byte policy is declared at include/void/native_storage.hpp:26 and enforced by
NativeStoragePlanner::finish(..., true), src/ir/native_storage.cpp:43-44. The staged
emitter freezes via finish(..., false) at storage_audit.cpp:745; its separate late
machine C check at :393 uses plan.policy, propagated through callers at :756-777.
The report still advertises the old budget at :870. This is not active public
native-array support: backend.cpp:101 still rejects native array lowering. These
checks remain intact as research/activation gates, but cannot be reactivated as a
universal production rejection of otherwise valid large-array programs.

A distinct, active source restriction exists at frontend/compiler.cpp:2428 (extent
above 65536) and :2434-2435 (expanded element multiplicity), plus enclosing-value
charging at :2483-2491. Independent IR inline_layout at ir/module.cpp:222-244 repeats
extent/multiplicity/aggregate guards. Existing foundation.cpp:137-138 explicitly
tests 65537 rejection / 65536 acceptance. A byte-policy-only correction would leave
this user-visible element/shape barrier. Defaults at compiler.cpp:1444/:1575 currently
expand per-element ConstantValue fields, so simply removing the guards is unsafe.

reference_paths at ir/module.cpp:73-89 separately bounds aggregate traversal work;
it currently does not traverse fixed arrays. Its real work/cycle protection must
be reviewed with future repeated reference metadata, not deleted blindly. The
interface discovery budget at ir/interfaces.inc:89 is unrelated to array size.
Research thresholds remain in tools/ci/array_abi_probe and array storage fixtures;
probe.py's 1..65536 count range and Part 5A workload bounds are experiment scopes,
not language limits. Section 21 lists detailed code/test/harness locations.

Before activation, investigate safe large fixed/growable storage on the existing
coordinated GC with no second memory-management system. No heap-backed fixed array,
copy-on-write rule or hybrid cutoff is selected. Review analysis complexity and
compact repeated/default/reference metadata across frontend, IR and shared tooling,
preserving finite layout, checked target bounds, actual resource protection, roots,
publication, safepoints, failure, readonly borrowing and reload contracts. Do not
rename or relocate the same arbitrary ceiling. Owner/wrench approval of architecture
and resource policy must precede implementation or production activation, followed
by megabyte-scale value/copy/GC/failure, complete-function, JIT/AOT/tooling and native
cross-platform verification. All useful CFG/ABI/stack research remains required.

### Verification and stop

Read-only source searches and call-site inspection distinguish the two guards and
existing coverage-before-delivery protection; no function-omission bug is claimed
fixed or newly demonstrated. Documentation diff/whitespace checks pass. Only the
six requested Markdown files change. No compiler/runtime, tests, harnesses, research
verifier, build configuration or CI files change; no builds or test matrix were run
for this documentation-only task. Existing verifier code/checks remain operational
and unchanged; CI #45 status is attributed to the owner, not a new local run.
Historical evidence is retained. No commits, pushes, branches or remote changes.
Stop for owner/wrench review before further architecture or implementation.


## 15 October 2026: Phase 23 large-array foundation, Stage 1

### Authority and scope

Read the owner's complete new Large-Array Storage Architecture Review, including
fixed owned-value semantics, compact shapes/defaults, future GC dependencies,
correctness obligations and six-stage roadmap. This is distinct from the earlier
hybrid certification report. Section 21's code audit is historical. Only Stage 1
is authorized: compact analysis and defaults plus independent IR validation.
CI #45's 7/7 jobs and 61 suites are owner-reported locked baseline evidence, not
hosted verification of these changes. No later-stage storage or activation approval
is inferred from the research direction.

### Implementation

- Reuse the existing structural Type/FieldType arena: exact canonical element edge
  and extent, no synthetic element fields. Remove the active 65536 extent and
  multiplicity barriers on the supported compact paths, including enclosing values.
- Add shared compact_value_shape analysis with memoized distinct descriptor walks,
  finite/depth checks and checked logical scalar-byte sums/products. Source count
  must fit the already approved A3 i64 public count contract. The scalar-byte result
  is a necessary lower bound, not a native/GC layout or target allocation estimator.
  Target padding, object overhead and allocator representation remain future checks.
- Pure defaults carry an exact repeat count and one prototype, or none for zero.
  Reference-free nested arrays and value defaults are supported in analysis/IR.
  Constant selection normalizes against logical extent and selects the prototype,
  rather than indexing a nonexistent expanded vector. Array-valued module constants
  no longer advertise a fabricated scalar numeric value to tooling.
- Existing fixed_array IR gains repeated-template mode. Independent validation
  recomputes shape/identity/extent, validates exact prototype arity/types and proves
  constant/aggregate/array-only prototype DAGs in SSA order. Calls and other effectful
  results cannot masquerade as pure repeated defaults. Repetition metadata on other
  ops, nominal reuse of array descriptors, arithmetic overflow and cycles reject.
  No frontend cached summary or unchecked count is trusted.
- Explicit scalar literal expressions keep their original ordered IR/effects.
  Custom-initializer repetition, reference-bearing shapes, growables and nonscalar
  literal/access operations remain explicitly deferred. Existing nominal/default
  recipe work bounds remain where the compiler still materializes actual fields.
  Distinct-shape/default-recipe work limits are not array-count or payload ceilings.
- Production backend/runtime/GC, native storage accounting, research certificate and
  encoded-CFG verifier are unchanged. Source native arrays still reject. No alternate
  ABI, GC descriptor, allocator, inline/heap cutoff or production policy is introduced.

### Focused evidence

Expanded the existing arrays_foundation suite, preserving its research byte-budget
boundary/overflow assertions and all existing suites. Positives include 2 MiB and
8 MiB scalar defaults, nested arrays, repeated value structs with nonzero defaults,
constant selection, exact identities and an INT64_MAX repeat of a zero-payload
shape. Logical huge-shape analysis is not a claim of successful runtime allocation.
The 1/2097152/8388608-byte cases each have exactly four IR instructions; the nested
fixture has fewer than 40, with no per-element fields or default arguments.

Negatives cover exact arity, incorrect repeat mode, effectful prototypes, metadata
on other ops, nominal/array descriptor confusion, finite cycles including zero
extents, malformed types, existing constant dependency failures, count/literal
representation, nested checked-product overflow and actual default-recipe exhaustion.
The last case is a small source DAG whose explicitly represented value-default
recipe duplicates fields; failure remains a structured resource diagnostic.

The first focused run caught my diagnostic catch wrapping a representability error
as unsupported_type; the catch scope was corrected and the intended diagnostic is
asserted. A cyclic forged array cannot have its claimed canonical identity: IR
rejects that identity, while a direct shape-walker negative separately verifies
finite-cycle detection. Neither failure was bypassed to obtain a passing test.

Linux LLVM 19.1.7 Debug and Release foundation tests pass, including 1 MiB native
stack runs of the expanded suite. Fresh Debug CLI checks under a 512 MiB address-
space limit measured (Linux wait4 ru_maxrss, separate child processes):

| Byte extent | Peak RSS KiB | Elapsed seconds |
| --- | ---: | ---: |
| 1 | 34952 | 0.0105 |
| 2097152 | 35000 | 0.0131 |
| 8388608 | 34568 | 0.0201 |

Timing/RSS are local observations, not performance thresholds. Deterministic IR and
metadata assertions provide the regression proof. Reproduce using a function with
`var data: [u8, N]` and `return data[-1] as i32`, CLI check, RLIMIT_AS=512 MiB,
and wait4 per child. The host lacks /usr/bin/time, so Python resource/wait4 was used.

The 8 MiB fixture's JIT and AOT attempts both reject with the retained native-array
message at normal/O2; AOT output sentinels remain unchanged. Rejected output was
never executed. A successful compact source check does not authorize native delivery.

### Remaining review boundaries

Before Stage 2 or later implementation, lock repeated GC descriptor ownership and
validation, rooted/private complete construction and failure, recursive fixed-value
copy/publication rules, growable identity/versioning, deterministic array ABI and
whole-build/reload publication. Production stack policy remains separate from
payload size. No machine/ABI/origin/CFG proof or platform evidence is discarded.
Reference-bearing analysis must not bypass the current reference-path/GC gates.
No heap-backed arrays, source native execution, ORC/LSP certification or later stage
is complete. Windows/macOS execution and hosted acceptance are not claimed.


### Completed validation and stop

Pinned SDK verified with `/usr/lib/llvm-19/bin/llvm-config --version`: 19.1.7.
Rebuilt all Debug/Release targets after the final source/IR checks, then ran:

| Check | Actual result |
| --- | --- |
| cmake --build build/ci-debug -j2; ctest --test-dir build/ci-debug --output-on-failure -j2 | Full build succeeded; 61/61 passed, 117.03 seconds |
| cmake --build build/ci-release -j2; ctest --test-dir build/ci-release --output-on-failure -j2 | Full build succeeded; 61/61 passed, 60.40 seconds |
| Strict sanitizers: arrays_storage_audit, arrays_foundation | 2/2 passed, 7.50 seconds |
| Strict sanitizers: interfaces_source, analysis, frontend | 3/3 passed, 21.60 seconds |
| Strict sanitizers: runtime_generated | 1/1 passed, 1.32 seconds |
| ulimit -s 1024; build/ci-debug/void_arrays_foundation | Passed |
| ulimit -s 1024; build/ci-release/void_arrays_foundation | Passed |
| git diff --check | Passed |

Sanitizer targets were rebuilt with VOID_SANITIZERS=ON (RelWithDebInfo); tests used
ASAN_OPTIONS=detect_leaks=1:abort_on_error=1 and
UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1, serially outside the ptrace sandbox
so LeakSanitizer could work. No sanitizer options/assertions were weakened. The two
full 61-suite runs include existing native AOT, GC, reload, source diagnostics,
LSP/tooling and the retained storage-audit/encoded-CFG suite. Interface crash capture
instrumentation is unchanged. No Windows/macOS run or complete six-configuration
matrix is claimed for this focused foundation checkpoint.

Updated proposal Section 22/current-status text, Language Bible implementation
status and Known Issues to distinguish compact analysis from future execution.
Historical Section 21 measurements/audit and 64 KiB research are retained. Only
compiler type/frontend/IR foundation, its existing test suite and documentation
change. No runtime/GC/backend implementation, CI, commits, pushes, branches or
remote modifications. Stage 1 is complete locally; stop for owner/wrench review
before Stage 2 or any production activation.


## 15 October 2026: Stage 2A repeated-GC/buffer architecture approval gate

### Authority and inspection

The owner reports CI #47 passed all seven jobs, 61/61 suites each, locking the
Stage 1 foundation. This entry attributes hosted status to the owner; no remote
verification or CI dispatch was performed. Reviewed the complete owner-supplied
Large-Array Storage Architecture Review, proposal Section 22 and the actual
include/void/{heap,runtime}.hpp, src/runtime/{heap,coordination,abi,reload}.cpp,
relevant LLVM root/staging lowering, and existing coordination/failure/invariant
tests. This is the new large-array report, not only the older hybrid verifier report.

Confirmed flat reference metadata, linear checked-store lookup, whole-payload
zeroing under Runtime's allocation lock, prepared-payload validation/copy under
construct_into's lock, registered exact Frame output requirements, NativeSafe frozen
root chains, stop-the-world mark-before-sweep with rollback, transactional runtime
metadata and code-generation lease lifetime. The existing failure test injects new
failures during graph work; invariant subprocesses require termination rather than
resuming after corrupt roots. These are design inputs, not newly reproduced defects.

### Proposed contracts for approval

Added ARRAYS_PROPOSAL.md Section 23, with exact proposed C++ API fragments, physical
metadata/payload layout, ownership/state invariants, negative tests and review steps.
Updated the proposal status and Section 22 to record Stage 1's owner-confirmed lock,
retaining original local evidence and the historical Section 21 audit.

- Runtime-owned immutable, compact element DAGs admitted transactionally into the
  existing catalog; per-allocation capacity layouts retain them without expanding
  repeated references or permanently interning every growth capacity. Domain,
  nominal/capability identity, geometry and checked arithmetic are explicit.
- One cursor/seek interpretation serves validation, tracing, stores and prepared
  references. Legacy flat offsets adapt into it; ordinary/string accepted layouts
  and existing unaligned memcpy reference offsets remain supported. Buffer/flat/
  string/nominal descriptor variants cannot be mixed as an unchecked union.
- Reserve and zero raw buffer storage through the same Heap while NativeSafe,
  before it contains any managed reference or GC identity. Reenter Managed and
  adopt the fully zeroed storage with an immediate precise private root, then
  populate in bounded chunks. This avoids payload-sized stack staging and a
  mutable uninitialized-tail tracing frontier. Raw allocation/zeroing can still
  have OS latency; it does not hold the collector-required mutex.
- Builders own small existing Frame roots, a complete prefix and in-element event
  cursor. Trace the full immutable capacity; spare/private nulls are not complete
  required-reference values. Child objects must be complete before linking.
  Nonfallible finish seals the prefix and publishes the output root without a
  safepoint, allocation, user callback or whole-payload copy. Failure preserves the
  old output and leaves only unreachable private garbage or a released reservation.
- Chunked source copying requires an independently rooted read lease; every runtime
  buffer writer honors it. BufferBusy is an explicit internal conflict before any
  write, with no waiting Managed across a safepoint. Readonly syntax alone does not
  prove alias stability; no public concurrency semantics or source borrow optimizer
  is introduced. Scalars and inline value bytes copy independently; managed/string/
  interface referents retain identity and capabilities. Owned-fixed/growable copy
  roles remain rejected until later representation/type-directed-copy review.
- Checked failure taxonomy, deterministic allocation-site sweeps, complete GC mark
  rollback, metadata/generation lifetime and legacy ABI preservation are specified.
  Reference-free buffers avoid payload scanning; reference-rich marking is still
  stop-the-world and proportional to actual pointer locations. No concurrent tracer,
  write-barrier system, allocator, size cutoff or numerical chunk default is chosen.

The proposed sequence is 2B schema/cursor proof, 2C private rooted byte buffers,
2D typed reference construction, 2E leased copying, then 2F local/native acceptance
and review. Approval of one step does not activate a later stage. New C++ buffer
services are not a source array ABI; all native-array, production certification,
ORC/LSP and ABI/origin/CFG gates remain. Future growable publication/versioning and
owned fixed-handle layouts remain separately reviewed obligations.

### Verification and stop

Documentation-only inspection and git diff --check; no builds, runtime tests or
benchmark experiments were run for this architecture checkpoint. Section 23's
acceptance table is a required future test plan, not passing-test evidence. It
includes independent small-layout oracles, millions-of-elements metadata counts,
all geometry/representation failures, private escape/incomplete publication,
collection at preparation boundaries, every allocation-site failure, lease/alias
conflicts, nested copy roles, retired metadata and 1 MiB/native platform checks.
Only ARRAYS_PROPOSAL.md and WORK_LOG.md change. No runtime/compiler code, ordinary
CI, research verifier, commits, pushes, branches or remote modifications. Stop for
owner/wrench approval before implementing any Stage 2 runtime change.

Design review refinements: zero_elements means explicit zero data, not execution
of language defaults. A field's nonzero default or user constructor cannot be
inferred from a physical GC schema; the Stage 1 recipe/source-construction proof
is a separate Stage 3 requirement. Failed builder operations poison the private
transaction and retry starts a fresh builder. Legacy buffer allocation/prepared
entry points cannot bypass the builder. Native storage facts stay LLVM-free;
lowering_layout_key compatibility is not a claim of verified LLVM emission identity.

Final documentation checks passed: whitespace, balanced new-section code fences,
new-section no-em-dash style, and an exact changed-file check restricted to the
two requested documents. Stop for owner/wrench architecture approval.


## 15 October 2026: Stage 2B compact GC schemas and shared cursor

### Authority and implementation boundary

Owner/wrench approved Section 23's Stage 2A direction and retained gates, authorizing
only Section 23.9 step 2B. Stage 1 remains owner-locked by CI #47, 7/7 jobs with
61/61 suites. No remote verification, dispatch or remote modification was performed.
Reviewed the complete supplied large-array report and actual transactional catalog,
Heap allocation/store/mark rollback, prepared construction, precise roots, interface
witness and invariant/failure fixtures. Stage 2A's documentation edits already
existed in this working tree and are retained with the subsequent approval status.

Added include/void/gc_schema.hpp and src/runtime/gc_schema.cpp. ElementSchemaSpec
uses native storage-domain facts and a discriminated topological DAG with scalar,
byte, managed/string/interface, record and repeat nodes. Owned-fixed/growable roles
remain explicit rejections. Checked admission proves finite topology, root coverage,
alignment, field identity/disjointness/containment, stride/base, i64 count and exact
representable repeated extent without enumerating elements. Duplicate structural
nodes share metadata; canonical keys never recursively embed child text. Iterative
comparison includes full shape, nominal/nullable/capability/domain/layout identity,
with no equal-size shortcut. Reordered topological node numbering is compatible.
All graph traversal work/storage depends on distinct nodes/edges or actual emitted
reference events, rather than a flat per-occurrence descriptor vector.

Extended Runtime's existing copy/validate/publish catalog with a three-input
admit_metadata overload and schema-only wrapper, canonical-handle lookup and exact
Runtime membership validation. Nominal, witness and schema additions commit together
or not at all. Deep-owned immutable metadata retains nominal/witness dependencies
without retaining Runtime, executable code, LLVM objects or generation leases.
Caller specs can be destroyed; retained cursors remain geometry-only after Runtime
retirement. Foreign handles reject even when their textual identities match.
Concurrent catalog admissions use the existing snapshot comparison/retry protocol.

One ReferenceCursor streams object-word events in physical order. Byte lookup and
logical field/index path lookup descend the same validated nodes; interface witness
words, padding, pointer interiors and invalid selections do not become references.
Cursor reset reserves a depth-bounded explicit work stack before changing state;
next allocates nothing. Reference-free repeats are skipped without element visits.
Zero-size logical fields do not shadow containing nonzero fields in physical lookup.
Legacy flat layouts adapt into the same service with alignment-one reference leaves,
preserving existing unaligned memcpy reference offsets and sorted normalization.
Heap allocation validation, tracing, checked stores, nominal layout normalization
and construct_into prepared-reference validation now consume this shared geometry.
The original object allocation/zero/copy/output-root publication sequence, ordinary/
string semantics, Frame/Slot/Handle ABI and collector mark-before-sweep/rollback
remain. Collector pending-object queue/mark ordering is deliberately unchanged.

### Focused regressions and measurements

Added tests/runtime/schemas.cpp and the gc_schemas CTest target, retaining all 61
prior suites for 62 total. Independent nested-layout and every-byte legacy oracles
compare cursor, byte lookup, logical selections and accepted stores; real unaligned
Heap and prepared Runtime objects survive collection. Interface tests distinguish
the object and witness words. Positive cases include all scalar storage domains,
empty repeats, zero-footprint children, duplicate sharing, alternate node numbering,
2,048-level finite shapes, detached metadata lifetime and two concurrent catalog
writers. Forged domains, identities, targets, recursive/forward/unused nodes,
reserved roles, overlap, duplicate logical IDs, misalignment/stride, arithmetic
boundaries, incompatible schema identity and foreign handles reject.

A million-reference repeated shape retains two nodes/one edge/depth two, with no
GC payload allocation. Independently seeded admission requested 1,886 metadata
allocation bytes for one reference and 1,917 for a million; this measures requested
bytes through the fixture's new hook, not RSS. Streaming a million events after
reset requests zero allocations. A billion-byte reference-free repeated shape
is skipped. A measured 48-allocation-site sweep covers every allocation ordinal in
a successful combined nominal/witness/schema transaction, verifies no partial catalog
publication and retries successfully. Separate incompatible-readmission and cursor
reserve-failure regressions preserve prior metadata/traversal state.

### Local validation

- LLVM 19.1.7, Linux x64, full Debug rebuild and CTest: 62/62 passed, 120.66 s.
- Full Release rebuild and CTest: 62/62 passed, 56.64 s.
- Rebuilt the final expanded schema regression and ran it again under 1 MiB Debug
  and Release stacks; both passed with identical compactness/48-site results.
- Explicit 1 MiB Debug/Release CTest selection: 9/9 each, 12.66 s / 3.95 s:
  gc_schemas, runtime_coordination, runtime_generated, runtime_callbacks,
  runtime_failures, runtime_aot_normal, runtime_aot_optimized, strings_source and
  interfaces_source. This includes generated JIT and independently linked native
  AOT comparisons. Existing mark-work failure/recovery reports eight injected
  failure positions and preserves every assertion.
- Full strict ASan/UBSan RelWithDebInfo rebuild and CTest: 62/62 passed, 146.99 s,
  including the final schema fixture and unchanged research/invariant/crash-capture
  suites. ASAN_OPTIONS=detect_leaks=1:abort_on_error=1 and
  UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1 remained enabled for generated
  build steps and tests. No sanitizer finding was reported.
- git diff --check, latest-section style/fence checks and exact changed-file audit
  passed. No Windows/macOS native execution is claimed.

The first sanitizer build's generated-code step failed because LeakSanitizer cannot
run under the desktop ptrace sandbox. The log states that restriction explicitly;
no runtime diagnostic or test assertion was suppressed. Rebuilt outside that sandbox
with detect_leaks=1:abort_on_error=1 and UBSan halt_on_error=1:print_stacktrace=1.
Build logs also contain the existing SDK ciso646 C++20 warning; no new schema-source
warning was observed in Debug/Release.

### Retained gates and limitations

Schemas are metadata only: no buffer kind/allocation, PendingBuffer, builder,
reservation, source arrays, new ABI, GC publication state or production certificate
is introduced. DAG admission is not permission to allocate payloads through legacy
Heap entry points. Native domains are host-derived, while lowering_layout_key is
compatibility data and not a proof of future LLVM/native emission correspondence.
Over-aligned geometry is representable but matching allocation/deletion remains a
Stage 2C proof. The catalog has no standalone interface declaration registry, so
an interface schema conservatively requires a prepared/admitted matching witness;
future typed buffer operations must still validate exact witness membership and
live value contracts. No existing readonly/source/reload contract is inferred from
geometry alone. Reserved owned/growable copies, source defaults, completeness,
read leases, private publication and queue optimization remain later-stage work.

ARRAYS_PROPOSAL.md Section 24 records the implementation, APIs, evidence and gates.
No compiler/backend/reload code, research verifier, 64 KiB experiments or CI workflow
changes. No commits, pushes, branches or remote mutations. Stop for owner/wrench
review before Stage 2C.


## 15 October 2026: Stage 2B ordinary Heap performance audit and flat adapter

### Authority, reproducibility and baseline

Owner reports hosted CI #48 passed all seven jobs with 62/62 suites, locking Stage
2B. Authorized a focused Release comparison with parent 3c69f66 and the smallest
safe correction if overhead was material. No Stage 2C or remote work is authorized.
The starting tree was clean at cb77d62. Added tools/bench/heap_legacy.{cpp,py} as a
standalone reproducible ordinary Heap audit, with no CMake or CI registration change.
Read-only git archive snapshots preserve parent and locked Stage 2B sources without
creating a worktree/branch, checking out files or changing Git state.

The final comparison uses identical production Release flags (-O3 -DNDEBUG,
C++20, RTTI, normal warnings), Clang 19.1.7 and native Linux x64 for all three
variants. The benchmark links actual Heap/schema sources directly, with no LLVM
backend dependency or artificial optimization suppression. Thirteen process samples
rotate parent, locked Stage 2B and optimized order on logical CPU 0 after one warm-up
process each. Per shape: 100,000 objects, ordinary payload 32 + refs*sizeof(void*),
0/1/4/16 unsorted unaligned reference offsets, actual backward managed edges,
three all-live collections and one all-dead collection. Every collection verifies
its exact object count. Separate instrumented executables measure new requests,
requested/retained metadata bytes and GC work allocation; timing binaries use the
normal allocator. Fixture root vectors and payload bytes are excluded from metadata,
while hash nodes/buckets are included. No RSS, platform-independent allocator size,
coordinated Runtime latency or source-array performance is inferred.

Command (one line): python3 tools/bench/heap_legacy.py --output
build/phase23-gc-perf-reviewed --samples 13 --objects 100000 --cpu 0 --label optimized.
The output results.json records exact commit IDs, compiler identity/commands,
source/header SHA-256 manifests, affinity, all individual samples, medians and ranges.
Earlier exploratory outputs (before, after, final, inline and pinned) remain under
build/phase23-gc-perf-*; the reviewed output is the final source-identical comparison.
No compilation/test load was launched until that final measurement completed.

### Diagnosis and smallest correction

The original flat adapter built a fresh ElementSchema DAG per allocation, including
canonicalization maps/strings, node variants, summaries, field-index vectors and
shared control blocks. This is material even for no-reference objects. Initial
seven-sample/50,000-object measurements showed about 5-11x allocation latency;
the final locked-snapshot comparison reproduces roughly 6-10x. Per-object new
calls rise from parent 2/4/4/4 to Stage 2B 11/18/22/26, excluding the same 14 bucket-
growth calls at 100,000 objects. Trace/event projection and per-object DAG destruction
also substantially increase live and dead collection costs.

Implemented a validated owned LegacyReferenceMap as the legacy mode of the shared
reference service, rather than constructing synthetic DAG nodes. Sorting, whole-word
bounds, duplicate/overlap rejection and unaligned memcpy offsets are preserved.
Heap nodes own this map once plus object kind and nominal descriptor, avoiding a
second TypeLayout offset copy. No cache, weak interning table, allocator or collector
change is introduced. Typed DAG canonicalization/domain/geometry/identity admission
is unchanged; its obsolete private legacy alignment bypass is removed.

Public flat shapes/cursors retain owned metadata across temporary, copy and move
lifetimes. Only Heap/Runtime can use a private scoped borrowed-map cursor, while
their validated map is stable and alive. The mark-loop cursor is local to each
object, ends before sweep and cannot escape or call user code. construct_into holds
its local map through poll/validation and transfers it only after traversal; its
existing output publication sequence is unchanged. next_offset projects the exact
same stream/index as next, avoiding construction of an unused typed event for GC.
Lookup, prepared validation and tracing share this service. Inspection of generated
code exposed a reset-helper call before the scoped-borrow constructor refinement;
no always-inline/noinline flags or ABI allowances were added to hide overhead.

### Final measurements

See ARRAYS_PROPOSAL.md Section 25 for full parent/Stage 2B/optimized tables. Compact
values below are in that order, per object (metadata includes bucket contribution):

| Reference words | Requested metadata bytes | Retained metadata bytes | Allocate ns | Live GC ns | Dead GC ns |
| --- | --- | --- | --- | --- | --- |
| 0 | 107.26 / 963.26 / 99.26 | 93.83 / 701.83 / 85.83 | 174.6 / 1020.9 / 170.1 | 44.0 / 107.2 / 40.0 | 54.9 / 204.9 / 47.6 |
| 1 | 123.26 / 1499.26 / 107.26 | 101.83 / 909.83 / 93.83 | 168.1 / 1363.9 / 155.2 | 66.5 / 330.0 / 70.4 | 74.6 / 406.3 / 73.4 |
| 4 | 171.26 / 1831.26 / 131.26 | 125.83 / 1053.83 / 117.83 | 167.0 / 1642.7 / 148.3 | 74.9 / 538.6 / 83.7 | 82.0 / 405.7 / 80.3 |
| 16 | 363.26 / 3153.26 / 227.26 | 221.83 / 1629.83 / 213.83 | 420.5 / 3192.2 / 372.7 | 144.9 / 843.0 / 134.3 | 118.5 / 541.4 / 94.4 |

Optimized new calls are 2/3/3/3 per object, with no additional per-object metadata
allocation for reference-free objects. Retained requested metadata is eight bytes
below the parent for each shape on this ABI. Allocation throughput is respectively
5.88/6.44/6.74/2.68 million objects/s versus parent 5.73/5.95/5.99/2.38. Live-GC work
requests match parent exactly: 54 new calls/6,291,432 bytes for three passes;
locked Stage 2B reference-bearing runs add three cursor allocations/144 bytes.

Do not claim universal GC parity: optimized live GC with one/four references is
5.7%/11.8% above parent medians. Their 13-sample ranges overlap (64.0-101.6 versus
63.2-92.4, and 78.2-120.0 versus 69.9-117.8 ns/object). Other live-GC medians and
all dead-sweep medians improve. This removes the material DAG amplification without
claiming zero residual cost, exclusive CPU access, frame-time or cross-platform
performance. No further collector/allocator/cache architecture is selected.

### Verification and retained boundaries

- Full Debug rebuild and CTest: 62/62 passed, 108.73 s.
- Full Release rebuild and CTest: 62/62 passed, 53.69 s.
- Expanded gc_schemas checks retain temporary/copy/move flat cursor ownership,
  common mixed typed/offset positions, logical paths, caller-offset mutation and
  alternating layouts. A measured three-site ordinary allocation failure sweep
  covers offset-vector/payload/map-node failure, preserves the rooted graph and
  confirms recovery. Existing geometry/forged-input, independent oracles, million-
  element compactness and catalog rollback assertions are retained.
- Direct 1 MiB Debug/Release schema runs passed: 1,878 versus 1,909 requested
  metadata bytes for one/million references, zero next allocations, and every one
  of the now 40 measured combined catalog allocation sites rolls back/retries.
  This is fewer genuine allocation sites, not a reduced ordinal sweep. Existing
  mark-work failure recovery passes seven failing positions with all assertions.
- Full strict ASan/UBSan RelWithDebInfo rebuild and CTest: 62/62 passed, 135.02 s,
  outside the ptrace sandbox with ASAN_OPTIONS=detect_leaks=1:abort_on_error=1 and
  UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1. No sanitizer findings.
- Nine-suite 1 MiB Debug/Release checks: 9/9 each, 11.73 s / 3.74 s, comprising
  gc_schemas, runtime_coordination, runtime_generated, runtime_callbacks,
  runtime_failures, runtime_aot_normal, runtime_aot_optimized, strings_source and
  interfaces_source. The full source/CLI/AOT and research checks remain in the
  62-suite runs; no Windows/macOS native performance or verification is claimed.
- git diff --check, new-section style/fence checks, benchmark source-hash match
  and exact changed-file audit passed. Benchmark artifacts remain in ignored build
  output; tracked tools and document tables reproduce/preserve the evidence.

The standalone benchmark is not a CTest timing threshold. No performance threshold,
source size ceiling, new source arrays, native execution/certification, Stage 2C
buffer allocation/reservation/builders, GC publication/queue changes or CI workflow
changes. No commits, pushes, branches or remote modifications. Stop for owner/wrench
review before Stage 2C.


## 15 October 2026: Stage 2C private rooted reference-free byte buffers

### Authority and scope

Owner/wrench authorize Section 23.9 step 2C only. Starting tree was clean at
4d25f4f; owner reports hosted CI #49 green, seven jobs and 62/62 suites. Hosted
status is attributed to the owner; no remote verification/dispatch/modification
was performed. Reviewed the approved large-array report, Section 23's reservation,
root/progress/publication contracts, actual Heap/Runtime/catalog/root/failure
implementation, and the locked Stage 2B legacy performance audit. No Stage 2D/2E,
source-native array, LLVM/ABI or production-certificate activation is authorized.

### Implementation and proofs

Added include/void/buffer.hpp and src/runtime/buffer.cpp. Runtime creates immutable
capacity-specific BufferLayoutRef from exact admitted ElementSchema pointers. Base,
positive stride (including zero-footprint elements), native alignment, i64 count,
checked extent rounding/products/additions and native address representation are
validated before allocation/pointer formation. Equivalent tokens compare exact
schema identity and geometry; equal-byte/foreign schemas do not substitute. Empty
capacity has a typed layout + null representation and no Heap object. Capacity
layouts are retained by live objects/users and released after collection, with no
permanent capacity interning.

Heap::PendingBuffer allocates/zeros raw storage NativeSafe outside Runtime's mutex,
using the existing ordinary/aligned new family and matching deletion. Actual pointer
alignment is checked before initialization; raw storage has no GC identity or
managed reference. Existing Frame/Slot ABI is unchanged. A thread-affine private
root uses a normal Frame/Slot plus a separate per-thread registration authorizing
exactly its word. Constructor links that null root before budget collection/Native
work, prepares depth-sized cursor storage NativeSafe, then reenters through the
existing Managed rendezvous. Heap map adoption completes all fallible insertion
work before immediate private-root installation under the same lock. Allocation
accounting increments exactly once; zero capacity consumes no successful allocation.

Buffer metadata holds layout/private-owner/complete/valid-prefix data using the
existing shared-owner slot selected by a checked object-kind tag. Ordinary object
nodes do not grow or allocate another map; the optimized legacy adapter remains.
Contains, Handles, ordinary stores and inspection reject private values; GC permits
only the registered builder root and rejects an ordinary root or heap edge to a
private value before sweep. A private-buffer count avoids extra target lookups in
ordinary tracing when no private construction is active. New buffer allocations
are impossible through legacy Heap::allocate/allocate_into/construct_into. Adopted
Stage 2C layouts are reference-free, so collection skips payload bytes entirely.
Existing queue ordering, mark rollback, string/nominal behavior and invariant policy
are preserved.

BufferBuilder implements ordered native scalar bits, byte/u8 streams and explicit
whole-element zero data. The internal DataCursor traverses the same admitted node
geometry/physical field indexes, with no recursion or multiplicity expansion.
Native scalar groups are 1/2/4/8 bytes, width/bool checked; IEC559 floating patterns
are preserved. Dense u8 paths coalesce/bulk-write safely. Byte and node work is
bounded by explicitly supplied quotas; polling occurs between chunks, including
metadata traversal for a deep element. Complete prefix/partial-event progress is
private and unforgeable. All reference/string/interface roles reject before raw
allocation, even in empty repeats. Physical zero validity is not proof of language
defaults or constructor execution. Native input spans have the approved host-owned
lifetime/provenance precondition across polls, not an implicit managed-copy lease.

finish_into validates the exact registered public output, completion and private
ownership, polls and revalidates under the mutex. Its noexcept seal/commit changes
only metadata/output/root words, with no payload scan/copy, allocation, callback or
safepoint. Any failed builder operation poisons it; old output is unchanged. Aborts
unlink/clear the small private root; adopted garbage is reclaimed normally, while
unadopted reservations free immediately with matching alignment. Wrong-state/thread/
LIFO destruction retains the existing fatal Frame policy. Inspection returns one
copied byte only from complete live storage; no mutable payload pointer, read lease,
managed deep copy or source array ABI is exposed.

### Focused tests, faults and NativeSafe witnesses

Added one gc_buffers suite (63 total, retaining all 62). Independent oracles cover
4 MiB dense input, nested record/repeat byte order, every scalar encoding, padding,
spare capacity, equivalent versus incompatible equal-byte identities, typed empty,
million zero-footprint elements, over-alignment, 2,048-deep construction and strong
capacity-metadata retention followed by reclamation. Negative inputs cover all
geometry/arithmetic/representation checks, foreign/null layouts/schemas, invalid
quotas and scalar bits/bool, wrong kind/order, incomplete/poisoned publication,
unregistered output and forbidden legacy buffer allocation/store/Handle/inspection.

Measured all successful-path allocation sites before ordinal sweeps: four with no
budget collection, six with allocation_budget=1. Every failure preserves the old
root, returns to a usable collection epoch, and reclaims all reservation/metadata/
node bytes before retry. Existing mark-failure and precise-root invariant suites
retain all assertions. Two subprocess cases require fatal handler exit 79 for a
forged ordinary private root and for an ordinary heap edge to a private object.

Linux test-only linker wrappers pause production memset after precisely half of a
poisoned raw reservation is zeroed (including __asan_memset for Clang sanitizers).
GC completes with no observable Heap identity while allocation/zeroing is NativeSafe;
then initialization resumes and the fully zeroed allocation is adopted/rooted.
No production callback or optimization suppression was introduced. Windows/macOS
use the native allocation pause/concurrent-phase witness without claiming exact
half-zeroed instrumentation; neither platform was executed locally. Collections
after adoption, in a partially written record, across chunks, before publication
and after abandonment pass. The local Debug chunk witness recorded 2,046 collections
and cooperative parks; this scheduling-dependent count is not a throughput contract.

Initial test wiring referenced Python3 before find_package had executed; CTest
reported Not Run, not a runtime failure. Moved the existing discovery before test
registration and reran successfully. No assertion, optimization or classification
was weakened to obtain a passing result.

### Validation and performance evidence

- LLVM 19.1.7, Linux x64 full rebuild and Debug CTest: 63/63, 115.11 s.
- Full Release rebuild and CTest: 63/63, 55.60 s.
- Full strict ASan/UBSan RelWithDebInfo rebuild and CTest: 63/63, 140.54 s,
  outside ptrace with detect_leaks=1:abort_on_error=1 and
  halt_on_error=1:print_stacktrace=1. No sanitizer finding. Final strengthened
  lifetime cases then passed gc_buffers/gc_schemas/runtime_failures/runtime_invariants
  again under the same strict settings: 4/4, 1.12 s.
- Explicit 1 MiB Debug/Release checks: 10/10 each, 13.25 s / 4.07 s, including
  final gc_buffers, schemas, coordination/generated JIT/callbacks/failures, normal/O2
  independently linked runtime AOT, strings and interfaces. Frame/Slot remain 32
  bytes on this native target; BufferBuilder is 264 bytes. These sizes are not a
  total machine-frame certificate; deep cursor storage and payloads use the heap.
- Release void_gc_buffers --measure, three local samples each for 1 MiB and 32 MiB,
  explicit 64 KiB/64-node fixture quotas, prewarmed map buckets and host input outside
  measured work: four allocation requests and payload + 152 bytes for both sizes;
  all measured storage is reclaimed. Requested bytes exclude allocator/RSS overhead.
  Median allocate+zero rates: 2,552.73 / 6,454.79 MiB/s; populate: 29,137.5 / 5,880.5;
  finish: 0.20 / 1.66 us; live collect: 0.33 / 2.48 us; dead collect: 0.41 / 426.82 us.
  Cold/warm effects are large: 1 MiB zeroing 1,990.77-49,726.5 MiB/s and dead collect
  0.17-81.31 us; 32 MiB zeroing 6,375.67-6,752.07 and dead collect 345.918-476.067 us.
  These instrumented local samples do not establish production latency, OOM or frame
  guarantees. Raw allocator/OS work and stop-the-world sweep can still block.
- Preserved the reproducible ordinary Heap benchmark. Ran heap_legacy.py against
  locked 4d25f4f for both baseline/control, nine 100,000-object samples per variant,
  same Release flags and CPU 0 affinity, with final-source SHA-256 identity verified.
  Results are build/phase23-2c-legacy-final/results.json. Calls and requested/retained
  metadata exactly match Stage 2B for 0/1/4/16 reference words (2/3/3/3 per object,
  plus common buckets). Retained bytes/object: 85.83/93.83/117.83/213.83. Allocation
  medians ns/object: locked 157.3/145.0/141.1/357.3 versus 162.8/161.6/153.1/384.8;
  live GC locked 40.1/70.9/87.0/135.9 versus 43.0/71.1/90.1/136.8. Do not claim zero
  timing cost or attribute all sample differences solely to new tag/privacy checks.
  The material DAG-per-allocation overhead stays removed; no cache/extra legacy
  metadata allocation is reintroduced. Measurements preceded full build/test load.

Proposal Section 26 records APIs, invariants, results and limitations. New runtime
code and tests only, with CMake source/test registration and a test-only Linux
linker witness; ordinary CI workflow, compiler/LLVM lowering, research verifier,
65,536-byte research fixtures and all existing gates remain unchanged. No commits,
pushes, branches or remote modifications. Stage 2C is locally complete; stop for
owner/wrench review before Stage 2D. Reference-bearing tracing/completeness, read
leases/deep copies, owned source fixed arrays, growable storage, source ABI/LLVM/
ORC/LSP/public certification and native hosted acceptance remain future checkpoints.


## 15 October 2026: CI #50 gc_buffers deterministic synchronization correction

Owner reports Windows 2022 Release 62/63, solely gc_buffers:
"no collection/poll interaction exercised"; Windows Debug 63/63. Inspected
native_and_chunk_collections, Runtime::collect/poll/park, Native/Managed entry and
wait_for_request. The failing assertion combined collections>0 and cumulative
cooperative_parks>0 without printing either. The supplied hosted failure therefore
cannot establish which counter was zero. No native Windows environment is available
locally; no Windows execution or hosted rerun is claimed.

Confirmed a test synchronization defect: request.count_down preceded the collect
loop, and the writer could set done before the collector requested anything.
Completed collections could also occur outside Managed construction. Two temporary
Linux Release fixtures controlled these legal schedules with latches and reproduced
the old assertion, first collections=0/parks=0, then collections=1/parks=0. Existing
runtime code was used unchanged. The second witness distinguishes completed
collections from actual cooperative interaction; the original message cannot.

Changed only tests/runtime/buffers.cpp and documentation. The writer now constructs
the same 4 MiB private buffer, populates a 4,096-byte prefix, then stays Managed
while an unattached observer witnesses a real single-collector request. Snapshot
checks require pending next epoch, unchanged completed epoch, two attached threads,
one parked collector and the private object. No writer poll or Native transition
occurs before the witness. Remaining write_bytes must cooperatively park and allow
collection; its snapshot is captured before finish_into or a Native join. Require
completed epoch, surviving private object and exactly one additional cooperative
park. Then publish, check first/prefix-boundary/last bytes, release and collect.
Separate assertions identify request/completion/park failures. Success prints
completed_collections=1 cooperative_parks=1. No production coordination change is
needed for this confirmed scheduling hole.

The observer uses wait_for_request with a five-second failure watchdog, not a
success-by-time check. No sleeps, repeated collector loop, extra input iterations,
scheduling yield or weakened assertions. Fatal private-root/private-edge checks,
allocation failure sweeps, independent byte oracles and NativeSafe/half-zeroed raw
reservation witnesses remain intact.

Verification and experiments:

- Controlled old Release schedules: both exit 1 at the original assertion with the
  explicit counters above. Temporary files are /tmp/p23-2c-original-controlled.cpp
  and /tmp/p23-2c-original-no-park.cpp; these are experiments, not tracked tests.
- An initial local observer prototype passed milliseconds::max to the timed-wait
  API. On this standard library the wait returns false immediately; throwing from
  the observer then blocks future unwinding while the writer is still Managed.
  Both initial Debug/Release runs were killed by the existing 150-second subprocess
  watchdog. Replaced this prototype with the finite failure watchdog and deferred
  assertions; all final results below use the corrected source. No runtime API
  change or hidden timeout increase.
- Rebuilt void_gc_buffers in existing Debug, Release and strict sanitizer builds.
  Nine selected suites pass: Debug 9/9 (2.87 s), Release 9/9 (2.03 s), strict
  ASan/UBSan RelWithDebInfo 9/9 (2.06 s). Sanitized execution outside the desktop
  tracing sandbox retains detect_leaks=1:abort_on_error=1 and
  halt_on_error=1:print_stacktrace=1; no sanitizer findings.
- With ulimit -s 1024: Debug 9/9 (2.72 s), Release 9/9 (2.03 s). Selected suites:
  gc_buffers, gc_schemas, runtime_coordination, runtime_generated, runtime_callbacks,
  runtime_failures, runtime_invariants, runtime_aot_normal, runtime_aot_optimized.
  gc_buffers includes both fatal subprocesses (required exit 79), four normal and
  six budget-triggered measured constructor failure sites. No full matrix rerun
  for this test-only correction; all existing suites remain registered.
- Final direct Release run prints exactly completed_collections=1 and
  cooperative_parks=1, with both allocation-site counts unchanged.
- Negative sensitivity experiment: compiled /tmp/p23-2c-no-write-poll.cpp, a separate
  copy with only write_bytes polls removed; production sources unchanged. Linked
  against the final fixture and existing Release runtime. It exits 1 with
  "chunk write did not complete the pending collection", despite finish_into's
  poll and the later Native join. The test cannot pass using those later events.

After adding the explicit <utility> include for std::pair, rebuilt all three
fixtures and reran gc_buffers: 1 MiB Debug 1/1 (2.66 s), 1 MiB Release 1/1
(0.25 s), strict sanitizer 1/1 (0.88 s), including fatal/failure subchecks.

Proposal Section 26.6 records the diagnosis and limits. No runtime/compiler edits,
Stage 2D implementation, Git mutations, remote changes or CI workflow edits.
Windows Release confirmation remains a hosted owner-controlled check. Stop for
owner/Wrench review.


## 16 October 2026: Stage 2D reference-bearing private preparation

Authority: owner/wrench authorize only ARRAYS_PROPOSAL.md Section 23.9 step 2D,
following 23.3-23.8. Stage 2C is owner-locked at cb601b4, CI #51 seven jobs,
63/63 suites each. Started with a clean working tree. No source array, value-copy,
lease, ABI/LLVM or production certification activation is authorized or performed.

Implemented through the existing services:

- ElementSchema derives zero_data_complete and centralizes the existing typed
  reference event projection for construction, stream and offset/path lookup.
  DataCursor consumes data/reference subtrees without expansion. BufferReferenceCursor
  streams absolute checked object/witness offsets over every immutable capacity
  element. Scratch depends on shape depth, not occurrence count.
- Added Runtime::RegisteredRootWord/root_word and RootedInterfaceSource, plus
  BufferBuilder::write_reference/write_interface. Tokens name an exact originating
  Frame word, Runtime and thread, not a cached referent or extended root lifetime.
  Validate public registration and same-Heap/complete membership before cursor polls;
  validate event kind/nullability/nominal ancestry/string kind/witness binding before
  the population poll; revalidate registration and reread/recheck afterward.
- Exact opaque witness membership precedes dereference. Nonnull objects must satisfy
  the admitted interface static view. Null interfaces require both words null and a
  nullable event. Both destination addresses validate before either word is written;
  the pair commits under one mutex with no poll/allocation/fallible split.
- RuntimeMetadata names the existing three-map transactional catalog, moved into a
  shared data-only declaration for Heap use. No second registry/admission transaction.
  Each buffer retains a catalog snapshot, refreshed on successful writes if admission
  extended it. Old ownership releases outside the write lock. Descriptor and witness
  identities/lifetimes persist without executable/generation pointers. InterfaceWitness
  moves declaration only; fields/type identity and Frame/Slot definitions stay intact.
- Heap's common typed-reference validator serves construction and marking. All buffer
  capacity positions are traced, including private incomplete elements and zero spare
  slots. Private null required values are trace-safe, not source-complete. Sealed live
  required references must be valid. Immutable requested_live detects private spare
  corruption; sealed valid_prefix detects public spare corruption. Neither controls
  trace extent. Nonzero spare object OR witness is an invariant error before sweep.
- Buffer-only queue filtering avoids a pending entry for each repeated referent.
  Final review made the set optional/lazy, so even an STL that allocates for empty
  unordered_set construction adds no legacy/null-only allocation. Engagement and
  insertion remain inside the existing mark rollback try block.
  Cursor/filter/queue failures use the existing reset-all-marks rollback before sweep.
  Legacy ordinary/string scans keep their borrowed cursor/queue and allocation costs.
  Reference-free buffers keep the O(1) payload mark shortcut. Adoption/root handoff,
  NativeSafe zero reservations, accounting, poisoning and scan-free atomic finish
  remain; seal additionally requires the exact immutable requested prefix.

Regression evidence:

- Independent small-layout oracle checks 45 outer/inner managed/string/interface
  positions and witness offsets, including reversed word order, padding and full
  spare capacity. Actual population bytes and graph/string reachability agree after
  source roots clear. Required Base accepts Derived; interface Base/Derived views
  remain distinct validated witnesses. Late type/witness admission during preparation
  exercises retained catalog handoff and tracing.
- Both million outer elements and million inner repetitions remain compact. Final
  automated allocation comparisons assert identical constructor calls/metadata bytes
  for 1 versus 1,000,000 repetitions with the same schema complexity. Node count is
  two for the repeated schema. Unused reference positions stay physically zero.
- Negatives: unregistered/expired/foreign-thread roots, foreign Heap referents,
  private targets, wrong nominal/string/interface kinds, bad quotas/order, required
  nulls, forged/mismatched/null witnesses and poisoned/incomplete publication.
  The old unsupported-reference constructor assertion is advanced to required-value
  incomplete-finish rejection; the empty repeated reference shape now positively
  finishes without inventing a live null required occurrence.
- Preserved the exact Stage 2C deterministic collection/poll test. Added a partial
  reference-element witness: a real pending request holds a Managed builder, the
  next interface write genuinely parks, and a test-only Linux x64 linker wrapper at
  Heap::collect(span) redirects a source slot after Runtime's rendezvous/root
  enumeration under its mutex. Both candidate objects stay separately rooted. The
  pair must use the reread post-poll object/witness; an incompatible nominal source
  must reject after the poll, poison the builder and preserve the old output. The
  snapshot proves completion before publication/Native join. CMake enables only
  this test-executable wrapper on the verified native Linux x64 C++ symbol; Frame,
  runtime and generated-code ABIs do not change. Other platforms retain mandatory
  request/poll/park assertions and report source-redirection instrumentation absent.
  No runtime hook/sleep, retry inflation, timing-based pass or disabled assertion.
  Final review replaced the initial allocator-based test prototype: a different STL
  could allocate during rendezvous before all mutators are safe. The Heap entry is
  a verified boundary, not an allocator scheduling assumption.
- Ten fatal subprocess modes require exit 79: the two existing private root/ordinary
  edge modes, plus public/private spare corruption, spare witness, required live null,
  wrong nominal/string kind, forged witness and private buffer-edge violations.
- Measured reference construction/partial-mark path has 11 successful-path allocation
  sites. Inject each ordinal; preserve previous output, no sweep/stuck epoch on GC
  allocation_failure, reclaim abandoned objects, then successfully retry a new builder.
  Existing four-site/six-budget-site failure sweeps and invariant tests remain.
- Initial fixture keys/word extents were rejected by existing witness-key/interface
  geometry checks; corrected the fixtures to the exact admitted encoding and derived
  footprint. Validators were not relaxed.

Actual local Linux x64 LLVM 19.1.7 verification, final runtime and compactness tests:

- Complete Debug build + CTest: 63/63, 82.41 s; Release: 63/63, 40.60 s.
- Complete strict ASan/UBSan RelWithDebInfo build + CTest: 63/63, 102.61 s, outside
  the desktop tracing sandbox with ASAN_OPTIONS=detect_leaks=1:abort_on_error=1 and
  UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1. No sanitizer findings.
- Explicit 1 MiB Debug/Release limits: 11/11 each, 11.99 s / 2.87 s. Suites include
  buffers, schemas, coordination/generated JIT/callbacks/failures/invariants, both
  independently linked runtime AOT variants, strings and interfaces. Native Frame
  and Slot stay 32 bytes; Builder is 328 versus 264 bytes in Stage 2C. These are
  object sizes and actual stack-limit execution, not machine-frame certification.
- Final native Release sensitivity witnesses link the final fixture/library with a
  temporary buffer.cpp that caches the pre-poll source (exit 1: source cached/pair
  assertion) or omits post-poll type validation (exit 1: invalid operation accepted).
  Both are /tmp experiments; tracked production objects are unchanged. The final
  no-Heap-wrapper Linux fallback executable also passes buffers.py, including all
  ten fatal modes. This tests the fallback branch, not Windows/macOS execution.
- Existing SDK <ciso646> warnings remain in builds; no new compiler warning, failed
  final test or test-coverage reduction. All 63 registrations remain unchanged.
  Full logs: /tmp/p23-2d-{debug,release,sanitizer}-ctest.log and corresponding
  build logs; small-stack logs: /tmp/p23-2d-{debug,release}-1m.log.

Reproducible cost audit, before full build/test load:

- New tools/bench/buffer_preparation.py uses read-only cb601b4 snapshots, native
  equivalent Clang 19 Release builds, CPU 0 affinity and records hashes/commands.
  Command: python3 tools/bench/buffer_preparation.py --baseline cb601b4
  --output build/phase23-2d-buffer-final --cpu 0. Artifacts include both byte logs,
  stage2d-references.log and identity.json. Final production/header/test-fixture hashes
  all match. Final audits ran sequentially before the final build/test workload.
  Requested bytes exclude allocator overhead/RSS.
- Both byte-buffer revisions request four allocations. Metadata is payload+152
  bytes locked versus payload+176 now, a fixed 24-byte catalog/boundary addition.
  Frame/Slot unchanged; no per-capacity metadata or permanent capacity interning.
  Counts exclude catalog admission and shared snapshot retention; buffers may extend
  lifetime of admitted data maps, without duplicating them per buffer.
  Three-sample 1 MiB allocate-zero/populate medians MiB/s: locked 2601.21/20161.3,
  current 2539.82/22482.0; 32 MiB: 7663.28/6850.80 versus 6934.57/6934.36.
  Finish/live-GC medians us for 32 MiB: 0.88/1.27 versus 1.55/1.70. Raw ranges
  are retained (current live-GC 1.64-2.06 us); no timing significance claim.
- Nullable native reference capacity 1 versus 1,000,000 both request four constructor
  allocations and payload+176 bytes. Live GC all-null: three work allocations/40
  requested bytes; duplicate references to one object: five/160, independent of
  repetition count. Unique objects still require proportional marking work.
  At one million events median live scan is 49.159 ms null / 71.569 ms duplicate;
  typed fill 293.173 ms and null build/zero 1.375 ms. This is stop-the-world GC,
  not hard real time. Incremental marking needs a separate barrier/architecture proof.
- Unchanged heap_legacy.py audit: --parent cb601b4 --stage2b cb601b4 --label stage2d
  --samples 9 --objects 100000 --cpu 0, output build/phase23-2d-legacy-final/results.json.
  Final timing samples ran after corresponding benchmark compilation, before full
  validation; final runtime/header/test-fixture hashes match. Both baseline/control
  are Stage 2C.
  New calls, requested/retained metadata, peak requested bytes and three-pass GC
  work counts/bytes match exactly for 0/1/4/16-reference ordinary objects. Allocation
  medians ns/object locked 166.60/156.92/150.33/372.07 vs 159.99/150.50/145.74/367.45;
  live GC 38.51/68.78/86.56/131.93 vs 38.53/66.77/80.82/129.38. Keep raw/control
  ranges; these samples prove no metadata amplification, not zero timing cost or a
  causal speedup. No legacy cache, per-allocation DAG or other allocator is introduced.

ARRAYS_PROPOSAL.md Section 27 records the contract/evidence; GC_RUNTIME.md links the
new runtime-only boundary. Native Windows/macOS, source arrays, leases/copying,
owned-fixed/growable storage, LLVM/generated ABI, ORC/LSP/public certification and
Stage 3 remain gated. No production size/scheduling ceiling or hard-latency/OOM
promise. No commits, pushes, branches, remote changes or dispatches. Stop for
owner/Wrench review before Stage 2E.

## 16 October 2026: independent official Actions Node 24 maintenance

Separate authorization from Stage 2D. Maintenance code scope is only
.github/workflows/ci.yml and .github/workflows/phase23-stack-probe.yml; the independent
evidence file is docs/CI_NODE24_MAINTENANCE.md. These files can be committed/reviewed
without runtime changes. No Stage 2D test command or matrix step is mixed into them.

Updated only existing official checkout@v5, cache@v5, setup-python@v6,
upload-artifact@v6 and setup-node@v5 references. Verified each upstream tagged
manifest uses node24 and retains the selected inputs. Compatibility notes require
Actions Runner >=2.327.1; actual hosted runner/action execution is not claimed.
Explicit npm cache selection and existing artifact defaults remain; Node 24.19.0
is still the editor toolchain, Python 3.12 and LLVM SDK 19.1.7 remain pinned.

Local exact-text check: compare both workflows with git show cb601b4:path after
only the five authorized substitutions. Both match byte-for-byte; ordinary CI has
exactly seven original matrix entries and the probe retains its three platforms.
All triggers, permissions, cache keys, artifact names/paths, shell/test commands,
sanitizer settings, conditions, timeouts and failure policies remain unchanged.
No deprecation-suppression environment flags, third-party action update, remote
modification or workflow dispatch. Owner controls separate commits/hosted acceptance.


## 16 October 2026: CI #53 macOS Debug reference-sweep diagnostic checkpoint

Request: investigate `reference failure sweep leaked storage` at `41f5b71`, without
weakening rollback/fatal/post-poll checks or changing Stage 2D semantics. Native
macOS ARM64 and libc++ are unavailable locally. The original combined assertion
and supplied log cannot establish which ordinal failed (including baseline),
remaining counts/bytes, allocation owner or real leak versus accounting defect.
These hosted facts remain unresolved; no remote action was performed.

Reviewed reference_transaction, PendingBuffer/adoption RAII, private-root cleanup,
mark rollback/retry, and Heap sweep/destruction. A concrete lifetime distinction:
GC erases object nodes but Runtime's unordered_map retains bucket capacity until
owner destruction. Reference adoption adds a third object after two pre-measurement
objects. Different standard-library growth behavior could retain a measured
bucket allocation. This is a hypothesis, not attribution of the macOS failure;
there is no evidence supporting a runtime patch yet.

Changed tests/runtime/buffers.cpp only: count outstanding native allocations as
well as bytes, split GC/native assertions, explicitly require successful final
collection, and flush baseline/ordinal/site/failure/source-release/native-count/
native-bytes/Heap-object diagnostics. An outer catch reports native survivors
after the Runtime and all local owners unwind, then rethrows. The original
pre-teardown retention failure is preserved and zero-byte native allocations
also fail. Logging occurs outside failure injection/recording. No ordinal is
removed; no warm-up, timing pass, platform exception or production fix is added.

Actual Linux x64 Clang 19/libstdc++ validation after rebuilding void_gc_buffers:
- Baseline: 11 sites; every injected ordinal 0–10 fails and rolls back correctly.
  Baseline and all ordinals end with native allocations=0, bytes=0, Heap objects=0
  in both Debug and Release. Logs: build/ci53-diagnostics/{debug,release}.log.
- Focused 11/11 suites pass Debug 12.08 s, Release 2.80 s, strict ASan/UBSan
  7.69 s (detect_leaks=1, abort_on_error=1; UBSan halt/stacktrace enabled).
- 1 MiB native stack: the same 11/11 pass Debug 12.14 s and Release 2.90 s.
  Includes gc_buffers and all ten fatal child modes, schemas, coordination,
  generated roots, callbacks, failures, invariants, normal/O2 AOT, strings and
  interfaces. Full 63-suite matrix was not rerun for this test-only diagnostic.
- Disposable diagnostic-control source under build/ci53-diagnostics deliberately
  retains a 37-byte RAII allocation in the measured baseline. It exits 1, reports
  native count=1/bytes=37/Heap objects=0 before teardown and count=0/bytes=0 after
  teardown. This proves the new reporting path preserves strict failure and
  distinguishes owner lifetime; it does not reproduce the platform failure.
- git diff --check passes. No runtime/compiler, workflow or generated ABI edits.

ARRAYS_PROPOSAL.md 27.5 records findings and the native evidence still needed.
Do not claim CI #53 fixed: the smallest deterministic diagnostics are prepared
for owner-controlled native verification. No Git mutations, commits, pushes,
branches, remote changes or dispatches. Stage 2E remains gated. Stop for owner/
Wrench review.


## 16 October 2026: CI #54 object-table lifetime correction

Owner/Wrench reports both native macOS ARM64 Debug and Release fail the baseline
ordinal -1 before injected failures: failed=0, native_count=1/native_bytes=40,
Heap objects=0; after Runtime destruction native count/bytes=0. The earlier
combined-check uncertainty is narrowed to native owner lifetime, not GC objects.

Traced ownership, rather than relying solely on matching destruction times:
Runtime/schema/catalog/root setup precedes measurement; baseline builder adoption
adds the third Heap map node. Cursor and constructor work are builder-local;
BufferState belongs to the collected node. Runtime roots, Heap pending work and
mark deduplication are collection-local and destroyed on return. Final empty-root
collection releases all payloads/nodes. No catalog update or Heap::roots_ growth
occurs in this transaction. The remaining measured owner is Heap::objects_'s
retained unordered_map bucket array; erase need not release it. It is runtime-owned
container capacity, not live managed storage or an instrumentation allocation.
The test was checking native storage before this legitimate owner died.

Added an independent Heap-only ownership witness plus a native unordered_map
insertion/clear control. Empty legacy-reference objects need no metadata or root
registration; after collecting payloads/nodes, the Heap object table is the only
remaining allocation owner. Compare retained count/bytes to the native bucket
control and demand zero after both owners die, for first insertion and the original
two-to-three transition. No hardcoded byte allowance, map-private access or platform
exemption. Local Linux Debug/Release first insertion: bucket count 1->13, both
Heap/control retain one 104-byte allocation, both zero after destruction. Two-to-
three: 13->13, zero new retained allocations. This directly exercises container
ownership independently of the original Runtime teardown. Native libc++ witness
output, including its actual bucket count, awaits owner-controlled hosted execution;
no local macOS SDK/run or five-bucket measurement is claimed. Source-owner exclusion
and supplied native evidence identify the 40-byte baseline retention as the Heap
object-table allocation; Section 27.6 records the argument and evidence boundary.

Changed test lifetime, not GC: reference_transaction still checks successful final
collection and zero Heap objects immediately. Its checked caller now requires
zero recorded native count AND bytes only after all Runtime/Heap/layout/frame owners
die, for baseline and every injected ordinal. All output-sentinel, mark rollback,
retry, fatal invariants and post-poll checks remain. Pre/post-teardown records stay.
Unexpected exceptions still reject; no allocations are ignored or reset away.

Added tracked deliberate-leak subprocess controls: retain 37 bytes or a zero-byte
allocation beyond owner teardown. Each must exit 1 with native_count=1 and the
strict native leak assertion (bytes=37/0). Cleanup occurs only after rejection,
then rethrows so the child remains failed; this avoids leaving artificial leaks
for sanitizers. buffers.py requires exact expected rejection, alongside all ten
existing fatal modes. No failure ordinal is skipped or made timing-dependent.

Actual Clang 19 Linux x64/libstdc++ results after rebuilding void_gc_buffers:
- Debug 11/11 focused suites, 11.87 s; Release 11/11, 2.82 s.
- Strict ASan/UBSan 11/11, 7.89 s, detect_leaks=1/abort_on_error=1 and
  UBSan halt_on_error=1/print_stacktrace=1, outside tracing sandbox.
- 1 MiB native stacks: Debug 11/11, 12.66 s; Release 11/11, 2.96 s.
- Includes gc_buffers, schemas, coordination, generated roots, callbacks, failures,
  invariants, normal/O2 native AOT, strings and interfaces. gc_buffers includes
  baseline plus all 11 measured ordinals (0–10), ten fatal modes, two leak controls
  and deterministic poll/park regressions. All real transactions end with Heap
  objects=0 before teardown and native count/bytes=0 afterward.
- Direct Debug/Release logs and negative-control records: build/ci54-lifetime/.
  No full 63-suite rerun or new native macOS/Windows validation is claimed.

Code changes only tests/runtime/buffers.cpp and buffers.py; documentation changes
ARRAYS_PROPOSAL.md 27.6 and this log. No production runtime/GC/compiler or workflow
edits, Git mutations, commits, pushes, branches, remote actions or dispatches.
Stage 2E remains locked. Stop for owner/Wrench review and native acceptance.


## 16 October 2026: Stage 2D hosted lock; Stage 2E authorization

Boss and Wrench confirm Stage 2D LOCKED at `97a9e49`, GitHub CI #55: all seven
jobs green, 63/63 suites each. This is owner-supplied hosted evidence. Stage 2E
only is now authorized under proposal 23.6/23.8/23.9: rooted stable-source leases
and distinct private copies. Stage 2F, source arrays, LLVM/ABI and production
activation remain gated. No Git or remote actions are authorized.


## 16 October 2026: Stage 2E stable-source copying — local implementation review

Implemented only the authorized Stage 2E runtime primitives after recording the
owner-supplied Stage 2D lock (`97a9e49`, CI #55, seven jobs, 63/63). Stage 2F and
all source/native-array, owned-fixed/growable child, compiler/LLVM/generated ABI
and production-certification gates remain closed.

Actual API/ownership:
- BufferBusy is the explicit internal lease-conflict exception. BufferReadLease
  takes a verified RegisteredRootWord and exact admitted BufferLayout, owns an
  independent ordinary Frame/Slot root, and derives its count from the sealed
  source. Noncopyable/nonmovable, same-thread Managed RAII/Frame lifetime rules.
- Checked mutex-protected reader counts commit with the root without polling or
  fallible work; destruction validates/decrements and unlinks the root. Multiple
  readers coexist; writers throw rather than waiting for readers. No wrap/underflow.
- BufferBuilder::copy_elements(lease, first, count) streams distinct private
  destination storage using existing DataCursor events, checked live-prefix/
  requested-prefix bounds and exact schema/profile identity. Different capacity,
  base and stride layouts are allowed. Bytes/u8 use bounded chunks; native scalars
  preserve bits; managed/string/interface roles preserve referent/witness identity.
  Interface pairs validate and commit atomically. No element-sized stack staging,
  expanded copy plan, metadata pointer or interior address survives into a poll.
- Each chunk reacquires source/destination under the existing mutex after polling.
  A caller can clear its original source word: the lease's root keeps it alive
  through NativeSafe reservation, collection and copying. Failed builders poison,
  leases/roots/reservations unwind, old outputs remain unchanged, and abandoned
  storage uses normal GC. Reserved owned-fixed/growable roles still reject at
  admission/preparation; there is no shallow-copy workaround.

Writer audit/fix:
- New checked logical-path scalar mutation and the legacy store's typed buffer
  managed/string-word branch enforce leases. Both preserve native geometry and
  required references; legacy interface half-writes, spare/private stores reject.
  These narrow embedding mutators require the explicit mutable fixture profile;
  readonly/unfamiliar profiles cannot gain write authority. This does not define
  source concurrency or a complete public mutation API.
- Registered output roots could otherwise alias payload words. Allocation,
  prepared construction, strings (including empty) and private-buffer finish now
  check this bypass before/post poll as appropriate. Leased aliases give BufferBusy;
  unleased untyped buffer aliases reject. Private builder address/zero/dense/finish
  paths also enforce the read-count invariant. No writable interior API is added.
- Heap buffer-object count is updated only after successful adoption and on
  destruction. Ordinary/string-only heaps take the output guard's constant-time
  counter fast path; with buffers present it scans Heap ranges under the mutex.
  This mixed-heap O(objects) guard is a documented scalability/lock-cost limitation,
  not a latency guarantee. No new address index, GC, allocator, numerical cutoff or
  production scheduling policy is selected in this checkpoint.

New proofs remain inside gc_buffers (all 63 suites preserved):
- 3 MiB u8 and byte-region copies with independent byte oracles, distinct geometry,
  bidirectional mutation independence, zero-capacity/zero-footprint data, multiple
  leases and a cross-thread writer reporting BufferBusy.
- Nested record/repeat scalar, required managed, string and interface content:
  independent offset/witness oracles, source mutation isolation, retained referents
  after original roots die. A real request overlaps a Managed partial destination,
  then copy polls/parks and collection completes deterministically. Aborted copies
  reclaim. Existing NativeSafe allocation/zeroing and post-poll tests remain intact.
- Private-source/alias, foreign/expired/unregistered root, foreign schema,
  wrong-thread lease, bounds, profile and owned-role negatives. Direct output aliases
  cover allocate, prepared construct, empty/nonempty string and finish paths.
  Trusted corruption controls restore source words after required-null, wrong
  managed/string kind and forged witness copy rejection; no partial output publishes.
- Successful reference-copy/partial-mark baseline measures 17 allocation sites on
  local libstdc++; all ordinals 0–16 fail, preserve sentinel/graph/epoch rollback,
  release leases/native reservations, collect abandoned objects and retry correctly.
  Native leaks check zero count AND bytes after Runtime/Heap owners die; GC objects
  must be zero immediately after collection. Existing 4/6-site byte and 11-site
  reference sweeps, ten fatal modes and two deliberate leak controls are retained.

Final complete native Linux x64 Clang/LLVM 19 validation:
- Debug 63/63, 91.26 s; Release 63/63, 46.66 s.
- Strict ASan/UBSan 63/63, 115.44 s: ASAN_OPTIONS=detect_leaks=1:abort_on_error=1;
  UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1. Initial sanitizer build under
  tracing failed at native AOT generation because LSan does not support ptrace;
  rerun build and tests outside that sandbox passed, with no suppression/settings
  relaxation. No production code change was made for that environment failure.
- 1 MiB stack Debug/Release: 11/11 affected suites, 12.07 s / 3.21 s. Includes
  buffers, schemas, coordination, generated roots, callbacks, failures, invariants,
  normal/O2 AOT, strings and interfaces. Full runs also preserve auditor, frontend,
  editor/LSP, existing stress and CLI coverage.
- Logs/measurements: build/stage2e-evidence and corresponding CTest LastTest logs.
  git diff --check passes. HEAD inspected read-only is the locked 97a9e49.

Requested metadata/cost witnesses (not performance thresholds): Release 3 MiB u8
copy/finish after destination construction 0.729445 ms; four allocations request
3,145,952 bytes, with destination payload extent 3,145,768 + 184 metadata bytes.
One vs 131,072 nullable references both use four allocations/184 metadata bytes,
payload 8 bytes vs 1 MiB. Single-sample copy/finish 0.00172/30.243 ms, two-live-
buffer GC 0.00123/13.0635 ms. Descriptor/copy metadata does not expand per element.
Samples ran amid other validation; allocator overhead/RSS, peak memory, pause
percentiles and production latency are not established. Read count adds one native
size_t to BufferState; ordinary/string tracing and legacy shape interpretation
are unchanged. GC remains stop-the-world and proportional to reference positions.

Proposal Section 28 records implementation, APIs and limitations. No native
Windows/macOS Stage 2E execution or hosted acceptance is claimed. Range-guard
scaling, comprehensive performance and target-native acceptance remain for review;
future compiler value-copy plans, owned/growable child roles and source activation
are separately gated. No workflow edits, commits, pushes, branches, Git mutations,
remote actions or dispatches. Stop for owner/Wrench review before Stage 2F.


## 16 October 2026: Stage 2E hosted lock; Stage 2F acceptance authorization

Boss/Wrench confirm Stage 2E LOCKED at `3cbe250`, CI #56, all seven hosted
jobs passed with 63/63 suites each. This is owner-supplied native hosted evidence.
Only Stage 2F runtime-foundation acceptance/review is authorized. Stage 2F remains
unlocked until the required owner-controlled hosted run passes. No source-array,
compiler/LLVM/generated ABI, later-stage, Git, CI-edit or remote action is authorized.


## 16 October 2026: Stage 2F local runtime-foundation acceptance, hosted pending

Recorded owner/Wrench lock: Stage 2E `3cbe250`, CI #56, 7/7 jobs and 63/63 each.
Stage 2F alone authorized; it remains UNLOCKED until required owner-controlled
hosted verification. No remote verification or dispatch performed.

Audited Sections 23.6/23.8/23.9/28, Language Bible arrays/value/readonly rules,
current log and actual schema/cursor/Heap/coordination/buffer paths. Runtime-owned
immutable schemas/catalogs remain transactional and compact; repeats use checked
geometry and distinct-node traversal. NativeSafe zero reservations, private rooted
adoption, whole-capacity tracing, sealed publication, lease roots/counts, all writer
conflicts and copied value/referent identities retain their contracts. No production
correctness defect reproduced; no runtime/compiler/generated ABI changes made.

Acceptance coverage additions, retaining all assertions and 63 suite names:
- gc_buffers now runs 32 deterministic budgeted record-copy cycles on every hosted
  platform: partial destination GC, lease conflicts, poisoned abort retaining old
  output, complete independent snapshot, post-lease mutation and two GCs/cycle.
- Copy fault coverage now measures budgets 0 AND 1; the latter directly exercises
  construction-triggered collection with a source lease active before reservation.
  Baselines measure successful allocation sites, then every ordinal is injected in
  a fresh transaction. Measured sites on both SDKs/all configs: schema admission 40;
  bytes budgets 0/1 4/6; references 11; copies budgets 0/1 17/29. Original sentinels,
  no-sweep/epoch rollback, fresh retries, after-owner native leak checks, ten fatal
  graph modes, two deliberate leak controls and post-poll checks stay intact.

Final native Linux x64 complete matrix (LLVM/Clang 19.1.7 in both cases):

| SDK | Debug | Release | Strict ASan/UBSan |
| --- | --- | --- | --- |
| system | 63/63, 128.29 s | 63/63, 67.15 s | 63/63, 149.16 s |
| official | 63/63, 117.93 s | 63/63, 60.95 s | 63/63, 140.18 s |

System /usr/lib/llvm-19 has LLVM RTTI ON/runtime -frtti; official archive under
.toolchains/ci-investigation/LLVM-19.1.7-Linux-X64 has RTTI OFF/-fno-rtti and matching
LLD. Full builds succeeded before acceptance. Strict settings match ordinary CI:
ASAN_OPTIONS=detect_leaks=1:halt_on_error=1:exitcode=87;
UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1:exitcode=86. Sanitizer builds/native
AOT generators/tests run outside ptrace so LSan works; no suppressed failures or
relaxed checks. Generated machine instructions/vendor SDKs are not automatically
ASan/UBSan-instrumented, distinct from compiler/runtime C++ coverage.

Repeat-until-failure stress (never repeat-until-pass): five selected runtime suites,
5 repetitions each in Debug/sanitizers and 10 in Release, both SDKs: all 200 suite
executions pass. Includes complete measured fault sweeps and genuine deterministic
NativeSafe/Managed GC, copy/lease/concurrent coordination regressions.

Real 1 MiB soft-stack executions, including frontend and 11 runtime/source suites:
- system: Debug 12/12, 14.88 s; Release 12/12, 6.91 s.
- official: Debug 12/12, 16.86 s; Release 12/12, 6.85 s.

Native C++ -fstack-usage probes use each runtime's actual build flags across buffer,
heap, schema and coordination units. Both SDKs: builder ctor 552/296 bytes Debug/
Release, lease ctor 184/40, copy 520/168, adopt 312/136, seal 104/8, guard 152/24,
Heap collect 1016/376; largest runtime-source entry catalog admission 2584/568.
No inspected runtime-source entry has a dynamic frame. Algorithms are iterative
by shape/element; owning heap metadata/cursors do not become payload-sized native
temporaries. Storage references are reacquired after polls. These are per-function
compiler reports, not total call-chain/ABI certificates, cross-target measurements
or a production array/stack numerical policy. Real small-stack execution complements
source/frame inspection. Existing physical depth 128/semantic depth 256 guards and
separate receiver-chain issue stay unchanged.

Added tracked Linux bench tools/bench/buffer_alias.{cpp,py}. The link-time ELF
probe measures the real production guard inside its caller's mutex, asserts two
calls/output, preserves optimization and records compiler/archive/source/CI hashes.
Rooted ordinary chains keep root-word lookup constant-sized. Empty-string output
allocates/collects nothing; zero/one/16 buffers and 0/1k/10k/100k/500k ordinary nodes
show scan scaling. Leased-output corruption controls reject. Five samples x64
outputs/population, sequential SDK runs, CPU 0/Ryzen 1700X, no agent build/test load.
Clock/wrapper overhead included, no guessed subtraction. Timing excludes mutex
wait and surrounding critical work; scheduler/cache/frequency are not controlled.

Final one-buffer medians: system 100k nodes 3.670 ms/output, 1.834 ms lock-held scan;
500k 14.124 ms/output, 7.061 ms/scan. Official 100k 3.641 ms/1.820 ms; 500k
13.866 ms/6.932 ms. Maximum observed scans 13.67/12.57 ms system/official.
Ordinary-only counter fast path remains about 0.134–0.136 us/output including probe
overhead. 16 buffers remain O(objects); layout/cache variations are not speedup
claims. This is a MATERIAL mixed-heap lock/scalability risk. Recommend separate
owner/Wrench review before a production-scale index/provenance optimization;
no index, weaker guard, arbitrary size cap or performance guarantee was introduced.

Artifacts: build/stage2f/manifest.json, acceptance/{summary.json, CTest logs},
frames/{commands.json,summary.json,*.su}, alias-{system,official}/results.json.
Proposal Section 29 contains exact tables, repro commands, audit boundaries and
hosted acceptance checklist. Existing seven-job workflow untouched: owner must
obtain native Linux x64 Debug/Release/Sanitizers, Windows x64 Debug/Release and
macOS ARM64 Debug/Release, all 63 suites and current Debug editor steps. New native
Windows/macOS Stage 2F results remain pending, not inferred from Linux/#56.

Changed only focused acceptance test, standalone benchmark and relevant docs.
Production include/src, compiler, CMake and ordinary/probe workflows match HEAD.
The pre-existing owner edit docs/WRENCH_CLANKER.md is preserved (including its
unrelated whitespace). Scoped diff/whitespace checks pass for this task's files.
No commits, pushes, branches, Git mutations, workflow edits or remote actions.
Stage 2F not locked; Stage 3+ and source-array activation remain locked. Stop for
owner/Wrench review.


## 16 October 2026: Stage 2F hosted lock and Stage 3 architectural review

Boss/Wrench confirm Stage 2F LOCKED at `d872889`, CI #57: seven hosted jobs green,
63/63 suites each. Record this as owner-supplied native hosted acceptance, not a
fresh remote query. It supersedes the pending-hosted status in the preceding local
acceptance entry and proposal Section 29. Historical Linux measurements, fault
coverage, cross-platform limitations at measurement time and all research evidence
are retained. No individual hosted logs or new platform timings were obtained here.

Authorized work: Stage 3 architecture only. Reviewed the supplied complete
Large-Array Storage Architecture Review, current proposal (Stage 1/2 contracts and
ABI/stack/encoded-CFG research), relevant Language Bible array/value/readonly/
construction/iteration rules, standing development/decision/known-issue contracts,
and actual frontend/IR, LLVM lowering, schema, Heap, coordination, buffer and
runtime-ABI interfaces/implementations. No native array implementation is claimed.

### Findings and proposed contract

Proposal Section 30 records the design, source reuse map, alternatives, proof
obligations, checkpoint sequence and acceptance matrix. Recommended representation:
one rooted payload handle for each nonempty fixed value, capacity=sealed prefix=n,
on the existing coordinated Heap; typed empty at n=0. Exact schema/profile/domain/
layout identity governs access. No inline-small threshold, replacement production
size ceiling, COW, second allocator or collector. Physical zero-footprint elements
retain checked nonzero stride; zero bytes do not imply a valid source default.

Eager distinct snapshots implement assignment, argument and return semantics.
Prepare each argument before evaluating later arguments. Use explicit caller-rooted
snapshot/result homes across private array-carrying calls, with existing checked
failure propagation and no payload-sized raw LLVM aggregates/byval/sret copies.
Array-free signatures and public adapters stay unchanged. Conservative copying is
the default; safe const borrowing/result transfer need independent effect/alias/
single-use proof, including conflicting-later-argument fallback.

Actual Stage 2 copy_elements copies physical data and shared referents. Reserved
SchemaOwnedFixed/SchemaGrowable are empty variants, rejected by schema admission;
copying a nested fixed value cannot merely reuse the managed-ref event. Recommend
extending the existing OwnedFixed role with exact admitted child type/extent,
checked single-use completed-child handoff and iterative independent child cloning.
Managed refs, strings and interface referents retain identity/capability/witnesses.
Growable roles remain rejected. Value structs and managed fields carrying arrays
need ownership-aware copy/store validation before their execution gates open.

A read lease freezes only one buffer. Nested snapshots must root and lease the
entire owned subtree before population, with parent links stabilized, checked
work entries and strict reverse-order Frame/lease cleanup. Ordinary referenced
objects are not deep-cloned/frozen. This is a proposed proof protocol, not a new
source concurrency guarantee or an already working runtime feature. Do not publish
an incomplete parent or install a private child edge. Complete children remain in
internal roots until transaction handoff; only the finished outer value is exposed
to source code. Failed preparation preserves previous outputs, releases private
roots/leases and leaves abandoned storage reclaimable by normal GC.

LLVM's current array type/op gates remain. Aggregate loads/stores/extract/insert,
functional array_set and direct returns cannot become raw handle aliases by changing
only physical type mapping. Independent IR validation needs explicit snapshot,
selected-place and handoff obligations. Shared target-derived schema/call/root
contracts serve JIT/AOT and tooling; no separate LSP estimator. Root handle words,
not expanded payload reference offsets; source expressions run in existing order,
and selected storage is reacquired after RHS effects/polls without restoring stale
siblings. Mark array allocation coordinated even without ordinary managed references.

### Review checkpoints and retained blockers

Recommended first authorization is **3A: scalar owned-value runtime proof only**:
exact fixed layout, compact default/literal construction, distinct clone, checked
scalar reads/writes and transactional registered outputs; no source activation.
Then 3B shared semantic/IR/helper/private ABI proof; 3C gated scalar source JIT/AOT;
3D owned nesting/richer references/ordered construction; 3E fixed iteration and
full acceptance. These are proposals, not implementation permissions or completed
milestones. Stage 3 completion requires nested value independence, not only scalars.
Stage 4 growables stay separate.

Unproved obligations remain explicit: exact owned-child catalog dependencies/finite
logical containment, single-consumption handoff, subtree snapshot and root/lease
lifetime, generated transaction cleanup, target ABI and root homes, dispatch and
managed-field adapters, and whole-build required-function completeness/publication.
An unsupported required function must fail the entire compilation, preserving the
previous artifact/generation. No silent omissions or unsupported-operation fallback.

Preserve immutable storage inventories and unique homes, conservative reconciliation,
encoded CFG/branch-retarget negatives, original hashes/sentinels and all existing
ABI/stack research. Heap placement does not prove emission-origin lineage, caller/
callee ownership, stack memory bounds, PUSH/tail encoding or final linked/ORC
relocated code. These gates remain separate and may require a separate approved
checkpoint before source execution. Public AOT certification remains inactive;
65,536-byte limits remain research-only, never a universal valid-program cutoff.

The Stage 2F mixed-heap output guard risk remains material (about 14 ms/output at
500k ordinary objects + one buffer in the recorded Linux benchmark). Stage 3 would
exercise it more. Retain the guard and negatives; seek separate owner review before
an index/provenance optimization for production-scale use. No replacement index,
latency guarantee, numerical limit or unrelated refactoring is introduced here.

### Verification and scope

This checkpoint performs source/document review and documentation consistency/
whitespace checks only. No builds, runtime/compiler tests or native witnesses were
run, appropriate to documentation-only architecture work. All proposed normal/O2
JIT/AOT, two-SDK Debug/Release/strict sanitizer, measured failure sweeps, deterministic
GC/lease/clone stress, real 1 MiB stack and seven-job native-platform acceptance
requirements are future evidence, not results of this review. CI #57 is the owner's
supplied evidence for the locked Stage 2 baseline.

Only docs/ARRAYS_PROPOSAL.md and docs/WORK_LOG.md were edited. No production code,
tests, CI workflow, Git operations, commits, pushes, branches or remote actions.
The owner's other files were left untouched. No source arrays, generated ABI or
Stage 3 implementation activated. Stop for owner/Wrench architectural review.


## 16 October 2026: Phase 23 Stage 3A scalar owned-value runtime proof

Stage 2F remains owner/Wrench LOCKED at d872889, CI #57, seven hosted jobs /
63 suites each. The owner approved proposal Section 30's architectural direction
and authorized 3A only. This checkpoint implements the scalar runtime layer, not
source arrays, compiler/IR ownership, generated ABI or later stages.

### Implementation and semantics

Added Runtime::fixed_default_into, fixed_literal_into, fixed_copy_into,
fixed_scalar and fixed_store_scalar in the existing runtime.hpp/buffer.cpp.
Reuse BufferLayoutRef's exact admitted element identity/profile/domain/layout and
immutable capacity as fixed extent; require a complete buffer with sealed live
prefix equal to capacity. There is no new object kind, descriptor arena, payload
header, allocator, collector, capacity interning or source size ceiling.

Scalar defaults/literals and copies feed the existing private BufferBuilder.
Literal bits are checked and consumed in order from stable non-managed native
input, with no payload staging. Copies acquire BufferReadLease and stream into
independent private destinations. Self-copy through one registered source/output
word retains the previous value in the lease until final publication. All fallible
work precedes the existing poll/revalidate/seal/output commit; failure leaves the
old output intact and unwinds private roots, reservations and leases. Abandoned
adopted objects become ordinary GC garbage, not leaked native reservations.

Scalar access validates registered roots, exact physical layout and complete fixed
extent. Reads copy native-width raw bits under the mutex without exposing an
interior address. Writes share the old buffer_store_scalar implementation through
a private helper, retaining width/bool/profile/lease checks and the real poll.
The fixed-extent condition is checked in its destination lookup after the poll too.
No unchecked scalar or alias-write path was added.

The subset is root Scalar for all eleven existing encodings, plus physically empty,
reference-free, zero-complete shapes for defaults/copies. Zero extent uses the
existing typed-null representation, with expected type supplied by its layout.
Nonempty zero-footprint elements retain checked positive stride and alignment.
These C++ embedding APIs use raw bits and uint64 zero-based ordinals; source
signed/negative index normalization, conversions and effectful constructors are
not implemented here. Ordinary references, strings/interfaces and nested owned
children cannot be smuggled into this scalar subset. Existing Stage 2 generic
buffer roles retain their prior behavior; owned-fixed/growable roles still reject.

### Focused proof and retained negative controls

Extended tests/runtime/buffers.cpp inside the existing gc_buffers suite, preserving
the 63-suite inventory and Python fatal/leak negative controls. Added:

- Independent full 4 MiB scalar-byte oracle, two-way mutation independence and
  self-copy; all eleven encodings, full high-bit u64, exact literal count, invalid
  scalar widths/bool values, out-of-range ordinals and wrong extents.
- Typed empty and distinct 2 MiB zero-footprint/aligned payload copies; rejection
  of scalar access to empty-data elements, nonscalar nonempty elements and a
  sealed buffer with spare capacity instead of a complete fixed extent.
- Foreign layout, unregistered/unlinked root and invalid quota rejection, preserving
  sentinel output. Existing private-root/heap-edge/spare corruption, forged witness,
  output-alias and post-poll reference regressions remain unchanged.
- Deterministic GC request while the writer remains Managed, followed by the actual
  fixed-store poll and exactly one cooperative park; successful post-GC copy/content.
- Successful-path measured allocation sweeps for default/literal/copy at budgets
  zero and one. Every measured ordinal must fail, preserve output, allow collection
  and retry; source mutation proves no leaked copy lease. Zero GC objects is checked
  after cleanup collection, zero native bytes/count after Runtime/Heap destruction.

The first focused attempt rejected test setup because the encoding loop admitted
schemas while Managed. Corrected that setup to use the established NativeSafe
admission scope; no production guard or assertion was relaxed. The failed attempt
and corrected focused run are retained under build/stage3a/initial-*.log.

The first system sanitizer build could not run an AOT generator's LeakSanitizer
under sandbox ptrace restrictions. Preserved system-san-build-in-sandbox.log and
summary-before-unsandboxed.json. Resumed remaining local validation outside the
sandbox with unchanged strict leak/halt settings; no detect_leaks=0, suppression
or skipped generator/test. This environment failure is not classified as a runtime
defect or a passing sanitizer check.

### Scope, performance and remaining gates

Existing mixed-heap O(objects) output-alias checks remain intact, including preflight
and finish validation. The Section 29.4 historical roughly 14 ms/output mixed-heap
Linux finding remains material and cannot be turned into a production latency
promise. No replacement index or unrelated optimization was implemented.

Protected manifest inputs (Heap, schema, coordination, generated ABI, compiler,
CMake and normal CI) match the prior runtime-foundation acceptance manifest.
Changed code only include/void/runtime.hpp, src/runtime/buffer.cpp and focused
runtime tests; documentation changes only ARRAYS_PROPOSAL.md and WORK_LOG.md.
No commits, pushes, branches, remote actions or CI edits. Stage 3B ownership/IR/
private ABI, nested owned-child copies, source native execution, production
certification and Stage 4 stay gated. Native Windows/macOS 3A results are pending
owner-controlled hosted verification; Linux is not substituted for those results.

### Final local verification

Final native Linux x64 verification on both supported LLVM 19.1.7 SDKs:

| Configuration | Full CTest | Real 1 MiB soft stack | Repeat-until-failure stress |
| --- | --- | --- | --- |
| system-debug | 63/63, 110.56 s | 6/6, 13.73 s | 5 suites x 3, 38.37 s |
| system-release | 63/63, 51.11 s | 6/6, 3.51 s | 5 suites x 3, 8.90 s |
| system-san | 63/63, 144.94 s | Not run | 5 suites x 3, 17.57 s |
| official-debug | 63/63, 140.98 s | 6/6, 13.73 s | 5 suites x 3, 38.39 s |
| official-release | 63/63, 45.74 s | 6/6, 3.49 s | 5 suites x 3, 8.92 s |
| official-san | 63/63, 144.32 s | Not run | 5 suites x 3, 17.50 s |

System SDK `/usr/lib/llvm-19`: RTTI ON. Official archive
`.toolchains/ci-investigation/LLVM-19.1.7-Linux-X64`: RTTI OFF, matching Clang/LLD.
Actual compiler versions and all six runtime flag sets were rechecked against the
prior acceptance manifest. The official sanitizer build used a freshly generated
Makefiles configuration with the same flags; its native ORC SDK check passed.
Strict ASAN_OPTIONS=detect_leaks=1:halt_on_error=1:exitcode=87 and
UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1:exitcode=86 apply to builds/native
fixture generators and tests. The sandbox LSan failure remains recorded separately;
remaining sanitizer execution ran outside ptrace, with no disabled leak checks.
Official RTTI-off LLVM cannot support vptr checks, as in the locked baseline.

All six configurations measure default/literal/copy at 4/4/4 allocation sites with
budget zero, and 8/8/10 with budget one: six successful baselines and **38 injected
failure ordinals per gc_buffers invocation**, preserving old outputs, collection
rollback, retry and post-owner native leak checks. Retained Stage 2 failure sweeps,
fatal invariants and deliberate nonzero-/zero-byte leak controls pass unchanged.
Stress repeats five affected suites three times per configuration (90 suite
executions), never repeat-until-pass. Full suites account for 378 executions;
real 1 MiB checks add 24. The six stack suites are gc_buffers, gc_schemas,
runtime_coordination, runtime_failures, runtime_invariants and runtime_generated.

Both SDKs' actual Debug/Release compiler flags plus -fstack-usage give these native
C++ entry frames, identical across SDKs:

| Entry | Debug bytes | Release bytes |
| --- | ---: | ---: |
| fixed_default_into | 488 | 392 |
| fixed_literal_into | 600 | 408 |
| fixed_copy_into | 680 | 520 |
| fixed_scalar | 264 | 40 |
| fixed_store_scalar | 152 | 40 |

The shared store implementation reports 584/120 bytes. Complete buffer.cpp reports
have no dynamic frames. These are per-function native C++ reports, not sums of
call chains, cross-target measurements or LLVM machine certificates. The 4 MiB
array and 2 MiB zero-footprint cases pass real 1 MiB Debug/Release execution.
No payload-sized native temporary or expanded element/root array was introduced.

Evidence: build/stage3a/{manifest.json,summary.json,measured-sites.json,progress.log},
per-configuration build/full/1m/stress and LastTest logs, initial failure logs and
frames/{commands.json,summary.json,*.su in configuration directories}. Reproduce
with `cmake --build <build-directory> -j 2` and the full
`ctest --test-dir <build-directory> -j 2 --output-on-failure` run; the six-suite
selection under a real 1024 KiB soft stack; and CTest's five-suite
`--repeat until-fail:3` selection. Exact command arrays and stack
limits are in summary.json; SDK paths/flags/source hashes are in manifest.json.
No Windows/macOS execution or hosted Stage 3A lock is claimed. All later checkpoints
remain gated; stop for owner/Wrench review.


## Phase 23 Stage 3B: private semantic/IR and call-proof checkpoint

16 October 2026. Owner/Wrench confirms Stage 3A LOCKED at `44fc8b6`, hosted
CI #58, all seven jobs passed with 63/63 suites each. Recorded this supplied lock
and reconciled the existing Markdown bot formatting before updating proposal
status and appending Section 32. Pre-edit documents are retained locally in
`build/stage3b/*.md.before`; historical measurements and writing remain intact.

Implemented an opt-in scalar owned-value contract in the existing shared IR:
explicit snapshots, borrowed loads, one-use argument/store/return transfers,
checked selection tokens and leaf-only commits. Independent validation rejects
shallow mutable aliases, repeated handoffs/commits, stale borrows, incomplete
selections, uninitialized homes at CFG joins, entry reexecution, mixed legacy/private
array calls and unsupported carriers. Source lowering remains functional and gated.

Added the exact emitter-derived private home/signature description and bounded
noexcept Stage 3A helper wrappers, plus an independently compiled six-argument
native C++ embedding witness. Existing runtime fixed operations, mixed-heap guard,
GC, schemas, BufferBuilder/read leases and fatal invariant controls are unchanged.
Expanded measured allocation-failure sweeps retain every original ordinal and add
bridge default/literal/copy ordinals with strict output, retry, roots/leases and
post-Runtime native-allocation cleanup checks.

Concrete stop boundary: the eight-pointer LLVM normal/O2 call fixture rejects for
two unproved incoming ownership objects (8 bytes each, entry offsets 0/8,
alignments 16/8). P=8/F=24/R=8/E=8/A=16/C=32 in the existing research certificate;
outgoing observation=16. No small-frame inference grants ownership. Sentinels stay
intact; rejected candidates are never linked or executed. Generated private calls,
Frame/root cleanup, final executable identity and cross-platform native acceptance
remain unproved. Public arrays, source ABI activation, Stage 3C+, nested ownership
and growables remain gated. Owner/Wrench review is required at this boundary.

Initial focused Debug verification passed arrays_foundation, gc_buffers and
arrays_storage_audit (3/3, 18.24 s). One earlier build invocation had a Clang 19
frontend crash with corrupted built-in-macro diagnostics; its log is retained in
`build/stage3b/stage3b-build.log`. A subsequent focused build completed. No compiler-source
cause is established; do not label that tool failure fixed or unrelated. Full
matrix and final evidence follow below.


Portability refinement before final acceptance: six homes plus result/failure fit
in ARM64 argument registers. The final negative machine fixture therefore uses
eight fixed argument homes plus result/failure (ten pointers) on every target,
without a platform exemption. Focused Debug arrays_storage_audit passes (8.71 s).
Final Linux normal/O2 evidence is P=8/F=40/R=8/E=8/A=16/C=48, outgoing observation
32, with four unproved incoming objects: MachineFrameInfo offsets 0/8/16/24 and
callee-entry-SP offsets 8/16/24/32. The initial six-home reports are retained as
initial evidence, not substituted for the final fixture. No native Windows/macOS
result is claimed. The test-only refinement requires rechecking the earlier two
Linux matrix entries against the final fixture.

The initial system Debug full run failed 62/63 at composition stress/optimized/JIT,
input -1 (expected 2002). Preserved log: system-debug-first-full-failed.log. The
same command passed 30 serial runs, 30 two-worker runs and one GDB run; the unchanged
official-SDK Stage 3A binary passed 30 two-worker controls. The complete composition
CLI suite passed five serial repeats (21.75 s). A fresh full Debug run passed 63/63
(106.12 s). This does not establish a cause, a fix or independence from this work;
the initial crash remains unexplained. Commands, binary hash, outputs and GDB log
are retained in build/stage3b/composition-repro. No assertion or production code
was changed to obtain that passing retry.


Validation scheduling correction: an additional final-fixture system sanitizer
full run overlapped the official SDK's large LLVM LTO links. It recorded
modules_cli timeout at its unchanged 180.08 s limit (earlier serial pass: 84.13 s);
numeric_cli passed in 136.45 s versus 56.04 s in the earlier serial run. CPU load
was high. This is evidence of contention, not proof of a runtime defect or permission
to raise the timeout. The overlapping recheck process group was terminated and its
partial log retained as build/stage3b/contention-system-san-full.log. The final
system sanitizer full run will be repeated serially after the official SDK work.
No test assertion, timeout, sanitizer option or compiler/runtime code was changed.


### Stage 3B final local verification and review stop

Final local evidence, LLVM 19.1.7 in both existing system and official Linux SDK
configurations. All six latest full runs pass all 63 suites. The system rows use
the final fixture refresh; official rows already include that fixture. Times are
observations on this host, not performance guarantees or isolated benchmarks.

| Linux configuration | Full suites | Full elapsed | 1 MiB eight-suite elapsed (s) | Seven suites, three repeats |
| --- | --- | --- | --- | --- |
| system-debug | 63/63 | 131.97 s | 22.7 | 64.18 s |
| system-release | 63/63 | 53.05 s | 5.92 | 16.14 s |
| system-san | 63/63 | 147.33 s | not run under sanitizer | 37.47 s |
| official-debug | 63/63 | 105.59 s | 22.45 | 64.40 s |
| official-release | 63/63 | 47.30 s | 5.6 | 15.12 s |
| official-san | 63/63 | 130.05 s | not run under sanitizer | 33.64 s |

Both sanitizer profiles retain ASan/UBSan, leak detection and halt-on-error:
`ASAN_OPTIONS=detect_leaks=1:halt_on_error=1:exitcode=87` and
`UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1:exitcode=86`. The existing SDK RTTI
profiles remain unchanged, including the official SDK's unavailable vptr check.
No sanitizer option or timeout was relaxed. The contended additional recheck timed
out; the final serial rerun passed modules_cli in 83.19 s against the same 180 s
limit (earlier serial 84.13 s). Preserve the partial timeout log as scheduling
evidence, not a passing run.

The eight affected suites ran under a real 1,048,576-byte soft RLIMIT_STACK in
Debug/Release for both SDKs (32 suite executions). The final system machine fixture
also passed separate 1 MiB reruns (8.51/3.57 s). Original seven-suite stress covers
126 suite executions; the final system audit fixture refresh adds three repeats
per system configuration (25.61/8.79/19.30 s; exact command records are authoritative).
All existing fatal, leak-control, rollback, post-poll and failure-injection tests
remain enabled. The fixed-value sweeps retain the original 38 measured positions
and add 38 bridge positions, with 12 baselines per buffer run. Captured logs confirm
76 positions: budgets 0/1 give 4/8 default, 4/8 literal and 4/10 copy for both direct
and bridge operations. Every failed ordinal preserves old output, retries and checks
root/lease cleanup, zero GC objects after collection and native allocation cleanup
after Runtime/Heap destruction. Earlier reference/copy sweeps are unchanged.

Both SDK compilers report static C++ frames: the six-home witness is 504 bytes
Debug / 360 Release; outer default/literal/copy/read/write wrappers are
56/120/104/104/104 Debug and 56/72/136/104/120 Release. Captured code/commands show
no payload-sized native temporary. These are individual C++ frames, not a complete
call-chain bound or a generated-code certificate. Stage 3A runtime header and buffer
implementation hashes match the prior Stage 3A manifest. The O(objects) mixed-heap
alias guard is unchanged.

All twelve final machine certificates (six configurations, normal/O2) reject the
four incoming ownership slots and preserve sentinels. No rejected object is linked
or executed. Public source arrays and production certification remain inactive.
The initial Clang frontend failure and initial Debug composition JIT crash remain
unexplained; passing retries do not establish their cause or a fix. Reproduction,
GDB, initial failure and timeout logs are retained. No Windows/macOS native result
or hosted Stage 3B acceptance is claimed.

Evidence: `build/stage3b/summary.json`, `final-refinement-summary.json`, command/log
files, `environment.json` (actual compilers, flags, LLVM paths and all suite names),
`manifest.json`, `measured-sites.json`, `certificates/`, `certificate-summary.json`,
`stage3a-preservation.json`, `frames/`, and `composition-repro/`. The source/test
manifest matches the final files. Validation scripts are copied into that evidence
directory. No commits, pushes, branches, CI workflow or remote changes.

**Review stop:** this is a bounded Stage 3B IR/runtime/layout checkpoint. The native
private calling proof is incomplete. Resolve incoming/outgoing ownership and
generated root/Frame/failure/emission obligations before native delivery; do not
advance to Stage 3C or infer permission from small frames or green runtime tests.


## Phase 23 Stage 3B blocker-resolution checkpoint, 16 October 2026

Continued Section 32 without compiler/runtime/test/workflow edits. Preserved the
original Clang frontend exit 139 and Debug composition JIT SIGSEGV evidence.
Detailed findings and next proof boundary are in ARRAYS_PROPOSAL.md Section 33.

- Saved Clang recovery input is valid UTF-8, 7,525,722 bytes. Three cc1 replays per
  LLVM 19.1.7 SDK pass with stable per-SDK object hashes. Three matching Debug
  system-driver compilations each of locked/current storage_audit.cpp also pass.
  The original built-in macro-buffer diagnostics and Lexer::SkipWhitespace trace
  remain unexplained. Recovery source is not proof of the failing in-memory buffer.
- After explicit owner authorization, downloaded only exact commit
  `44fc8b686b1e28665455b37a1a825f7ba05e186e` into isolated /tmp storage and built its
  system LLVM 19.1.7 Debug voidc. Archive SHA-256:
  `d3acbd304282429be77604386dd6d37c53b57f1427c028e4fc847a9c09781b4f`.
  No Git access, working-tree replacement, remote modification or branch operation.
- Current system/official Debug and isolated locked system Debug each pass 30
  serial, 30 two-worker parallel and 10 real 1 MiB runs of the same optimized
  composition stress source with argument -1 and exact output 2002. Core dumps
  were enabled; no crash/core was produced. This does not fix, classify or explain
  the original crash, or exclude a regression. No official-SDK locked build claimed.
- Normal/O2 compile-only MIR establishes four incoming pointer words at entry SP
  +8/+16/+24/+32, forwarded by two packed vector loads/stores into the existing
  32-byte outgoing area within F=40. Local pointer home is at post-prologue SP+32.
  P/R/E remain 8/8/8; research C=48. No rejected object is delivered or executed.
- Native GDB inspection of the existing C++ six-home embedding witness confirms
  four stack slots at those offsets, balanced return SP, successful rooted copy
  result and normal full gc_buffers completion. Its result/layout/quota/failure
  roles differ from the LLVM fixture's g/h/result/failure and cannot certify it.
- Actual blocker: LLVM fixture has no caller definition, bridge body, generated
  Frame/Slots or failure cleanup. Current machine observations do not bind a stable
  call-site/formal ABI assignment and root ownership to final encoded bytes.
  Packed forwarding also loses four separate identities in the present report.
  General incoming acceptance would require the retained ABI-ledger/origin proof,
  with exact transformations and negative identities, not an offset-based exception.
  No narrow safe correction was established; all four rejections remain enabled.

Fresh local verification completed successfully. Every profile's four targeted
suites pass before its full 63-suite run; no compiler/runtime/test code changed.

| Linux configuration | Full suites | Full elapsed (s) | 1 MiB eight suites (s) | Seven suites, three repeats (s) |
| --- | --- | --- | --- | --- |
| system-debug | 63/63 | 109.56 | 22.31 | 63.90 |
| system-release | 63/63 | 53.60 | 5.98 | 16.28 |
| system-san | 63/63 | 146.67 | not run under sanitizer | 37.10 |
| official-debug | 63/63 | 105.57 | 22.30 | 63.79 |
| official-release | 63/63 | 47.56 | 5.52 | 14.99 |
| official-san | 63/63 | 140.12 | not run under sanitizer | 35.44 |

Strict ASan/UBSan and leak detection remain enabled in both SDK sanitizer profiles,
using the unchanged Section 32 flags and SDK RTTI configuration. No timeout,
assertion, failure ordinal or negative certificate was relaxed. The four actual
1,048,576-byte Debug/Release stack runs cover 32 suite executions. Seven suites
repeated three times in all six profiles cover 126 stress suite executions.
All twelve newly copied normal/O2 reports retain four incoming-ownership errors,
zero incoming credit and unchanged output sentinels. None was linked or executed.
The native witness log confirms all 76 measured fixed-value failure ordinals and
12 baselines; other existing failure and fatal controls remain in the full suite.
No crash recurred, so both original causes remain unresolved after this independent
baseline comparison. Generated incoming/root/failure/emission provenance remains
the review blocker. No Stage 3C or production/native-array activation.

Evidence: build/stage3b-blockers, including archive/build identity, source hashes,
Clang commands, per-process composition results, compile-only machine snapshots,
slot-mapping.json, native-cpp-homes.json and validation command/log records.
All existing negative controls, failure ordinals, fatal invariants and strict leak
checks remain enabled. Stage 3C and later checkpoints remain locked. Stop for
owner/Wrench review; no hosted acceptance or Windows/macOS execution is claimed.

Documentation preservation note: the Language Bible changed relative to the initial
checkpoint checksum while this work was running. No command in this checkpoint
wrote it. Its latest contents are preserved untouched; initial/observed hashes
and a read-only evidence copy are retained. Do not restore the earlier file or
attribute that separate replacement to this investigation.


## Phase 23 Stage 3B: private ABI header consolidation, 16 October 2026

Consolidation succeeded without a dependency cycle or a new dependency. Inspected
both headers, both ABI implementation files, the three fixed-header consumers
and the existing runtime/execution/buffer include chain. The removed fixed header
already included runtime_abi.hpp; that header already provides every required type.
Both groups are private same-build C-linkage helpers, not a public FFI boundary.

Moved all five vd_fixed_* declarations and their scalar-proof/lifetime contract
comment into the existing extern "C" block in include/void/runtime_abi.hpp.
Removed include/void/fixed_runtime_abi.hpp. Updated only the include directives in
src/runtime/fixed_abi.cpp, tests/runtime/buffers.cpp and tests/runtime/fixed_call.cpp.
The implementations remain in fixed_abi.cpp. All five declaration lines are
byte-identical and unique in the consolidated header; implementation bodies and
test assertions are byte-identical after accounting for those include substitutions.
C linkage, signatures, noexcept, failure translation and calling conventions are
unchanged. No forwarding header, duplicate declarations or abstraction was added.

Focused Linux system LLVM 19.1.7 verification, without the full six-configuration
matrix or full 63-suite rerun:

| Configuration | Focused suites | Elapsed | Real 1 MiB stack suites | Elapsed |
| --- | --- | --- | --- | --- |
| Debug | 8/8 | 38.64 s | 3/3 | 19.26 s |
| Release | 8/8 | 9.94 s | 3/3 | 3.78 s |
| strict ASan/UBSan/leaks | 3/3 | 11.22 s | not run under sanitizer | n/a |

Debug/Release cover gc_buffers, arrays_storage_audit, runtime_generated,
runtime_failures, runtime_invariants, runtime_coordination, strings_source and
interfaces_source. The sanitizer selection and the two real 1,048,576-byte stack
runs cover gc_buffers, arrays_storage_audit and runtime_generated. The sanitizer
options remain detect_leaks=1:halt_on_error=1:exitcode=87 for ASan and
halt_on_error=1:print_stacktrace=1:exitcode=86 for UBSan. All selected target builds
pass. Existing buffer failure sweeps, fatal/leak controls and native private-call
witnesses remain enabled. No new test logic was needed for declaration relocation.

All six fresh normal/O2 reports across these three configurations retain exactly
four unproved incoming ownership slots, zero incoming credit and intact delivery
sentinels. No rejected object was executed or delivered. No machine-certification
rule, root/Frame contract, source-array gate or Stage 3C boundary changed.

LANGUAGE_BIBLE.md and WRENCH_CLANKER.md are byte-for-byte unchanged. The Wrench
handoff's historical mentions of the removed header are intentionally untouched.
No CMake, workflow, Git or remote changes. Evidence and exact scoped diffs are in
build/stage3b-abi-header-cleanup. Stop for Wrench review.


## Phase 23 Stage 3B: private call-envelope integration, 17 October 2026

Continued the completed research pilot at baseline
`7abc6249a5e1828fd49232a868dcc300b13b7894`; no investigation restart or production
ABI replacement. The original pilot notes/evidence remain available in
`tests/research/call_envelope/PILOT.md` and `build/stage3b-envelope/review`.

Resolved the Clang fixture's O2 tail rejection with an observable caller
continuation: branch on failure, read the returned value, process it for the
consumer (100 on success, 0 on failure). The unchanged auditor accepts normal
and O2. No dummy operation, tail-call disabling, gate relaxation or LLVM patch.

Added an explicit research emitter entry in the existing backend and a narrowly
gated branch within `lower()`. It runs existing owned-IR validation, derives the
logical signature and exact profile through `fixed_call_layout`, checks POD
sizes/alignments/offsets against target DataLayout, and constructs actual LLVM
caller/callee functions. The only admitted logical body returns argument 0 from
eight identical fixed-u64-array parameters. The test driver obtains canonical
types from frontend analysis and constructs this owned IR explicitly; this is
not completed native VOID source-array lowering. Public source/JIT/AOT entrypoints
never select this path. Their existing array rejection remains in place.

Generated homes, Slots, Frames, envelope lifetime, transactional copies, ordered
source effects, failure branches, result processing and cleanup are explicit.
Runtime assertions establish actual registration, independent snapshots/results,
callee roots before collection and expiration of both Frames. Closed C++ authority
records are fixture support, not an added compiler-wide ownership system. No
payload-sized stack allocation or offset-derived ownership is accepted.

Both generated modules receive storage delivery certificates. Callee: P/F/R/E/A/C
= 352/392/352/8/16/400 in normal and O2. Caller normal:
464/552/464/8/16/560; caller O2: 464/536/464/8/16/544. Each has zero incoming
slots, zero outgoing argument bytes and no tail call. Full revised Clang and
baseline comparison appears in ARRAYS_PROPOSAL Section 34 and the research README.
The original ten-pointer fixture, storage auditor and encoded-CFG verifier are
unchanged; both ten-pointer profiles still reject four incoming ownership slots
and preserve sentinels. Raw Clang profile mismatch remains a recorded rejection;
compiler-generated LLVM needs no target-attribute adaptation.

Focused system LLVM/Clang 19.1.7 verification:

- The runner passes 64 revised Clang-boundary and 48 compiler-generated case
  executions across normal/O2 and actual 1,048,576-byte stack limits. It checks
  certified delivery plus exact object SHA-256 before linking/execution; no
  uncertified candidate is executed. Negative admission tests preserve output
  sentinels for unsupported bodies and absent ownership contracts.
- The existing full storage-auditor fixture/negative suite passes unchanged.
- Five rebuilt suites pass (5/5, 11.27 seconds locally): gc_buffers,
  arrays_foundation, runtime_generated, runtime_failures and runtime_invariants.
  Existing fixed-helper failure sweeps and fatal controls remain enabled.
- Another 56 cases pass with strict ASan/UBSan and leak detection against the
  existing sanitized Runtime. Only accepted boundary objects are linked. Hooks
  and Runtime are instrumented; emitted boundary objects are not, so this is
  not complete generated-code sanitizer coverage. Sandbox LeakSanitizer process
  inspection failed; the unrestricted rerun passes with detect_leaks=1,
  halt_on_error=1 and unchanged strict exit codes (ASan 87, UBSan 86).

One initial generator run crashed before writing IR because the new fixture
created a global string with no insertion block/module argument. Corrected the
fixture to provide its module explicitly; subsequent normal/O2 generation,
certification and execution pass. No rejected/crashed-generation object was
linked. Earlier attempted-run logs are retained, not represented as successful.

Semantic and storage evidence remain distinct. No independent final-byte proof
of the complete linked ownership/root/failure protocol is claimed. Arbitrary IR,
new-path exhaustive allocation-failure sweeps, nested/managed arrays, selected-
place commits, general source execution, ORC, recursive/concurrent registry
semantics and other native targets remain outside this checkpoint. Generated
cleanup is checked by token expiration; the separate Clang fixture retains
missing-cleanup injection/repair before stack homes expire.

Evidence: build/stage3b-envelope/integration-final and integration-san, with
commands, original/optimized LLVM, identity manifests, certificates, source hashes
and native logs; affected build/test logs are in the parent directory. The review
ZIP contains scoped source changes, unified patch and focused evidence. No
six-configuration matrix, hosted CI, branch, commit, push or remote write.
LANGUAGE_BIBLE.md and WRENCH_CLANKER.md were not edited; the preexisting Bible
change remains preserved and is excluded from the sprint patch. Stop for owner
and Wrench review before production integration or Stage 3C.


## Phase 23 Stage 3B: generated ownership authority, 17 October 2026

Continued the existing uncommitted integration at accepted baseline
`7abc6249a5e1828fd49232a868dcc300b13b7894`. Preserved the pilot, preceding
integration source ZIP, passing reports and native logs. Current evidence is
`build/stage3b-ownership/review`; preceding evidence remains under
`build/stage3b-envelope`. No production ABI or Stage 3C activation.

Review found that `research_validate` and `research_context_validate` still made
the C++ active-call registry an authorizer. Removed both calls and definitions.
The registry now observes/injects faults only. Generated code checks the exact
compiler identity, ordered envelope fields, per-call binding, epoch, context and
actual live-Frame membership. The private callee has internal linkage.

The compiler now constructs the fixture wrapper in existing owned IR and invokes
shared `ir::validate`: independent fixed_snapshot values, later selected-place
source mutations, and exactly-once consumption at the call. No second ownership
engine or structural type analyzer was introduced. The compiler maps each IR
snapshot to one SSA home used by its Slot, envelope field and compiler-generated
CallBinding. CallBinding is bounded per-call stack storage, not a second runtime
registry or permission minted by the test harness. Freshness comes from the IR
contract and actual completed copies, not registration or matching offsets.

Added exact-Frame root_word validation using the existing Runtime Frame list.
Root validation additionally binds the compiler-derived scalar encoding/extent
to Runtime's admitted schema metadata. This fixes an actual type hole: a same-
sized i64 schema could otherwise substitute for u64. A separate output-place
helper preserves the existing ability to publish into a registered null result
home, without reading it as an initialized fixed source. The generated callee
validates its own homes before collection and revalidates the envelope before
transactional result publication. These helpers are not registered with ORC and
never grant incoming ownership.

The admitted callee now returns a validated argument index rather than a
hardcoded zero. Arguments 0 and 3 are tested: snapshots yield result values 7
and 10, processed by the real caller continuation into 8 and 11. A valid but
unsupported additional-snapshot body and missing ownership still reject.
General caller/source-array lowering is not implemented: the wrapper remains a
synthesized, narrowly admitted research workload with eight same-layout u64
arguments and explicit source effects.

A supplemental private IR gate checks original snapshot/Slot/envelope/binding
and callee-load mappings, exact ordinal/Slot storage fields, the selected return
source, required operations and cleanup dominance/postdominance. It checks 52
required operations after optimization, including the private handoff itself.
The final handoff check corrected a gap where helper calls could remain in an
unused callee despite deletion of the actual call. The protocol report explicitly
declines optimized store-lineage and final-byte ownership certification. Existing
immutable storage inventory, machine observer and encoded-code gates are unchanged.

Fresh system LLVM/Clang 19.1.7 verification:

- Four generated objects certify: arguments 0/3, normal/O2. Callee normal
  P/F/R/E/A/C = 352/408/352/8/16/416; callee O2 = 352/424/352/8/16/432.
  Caller both profiles = 592/664/592/8/16/672. All have zero incoming slots,
  zero outgoing argument bytes and no tail call. Charges remain research-only.
- 280 boundary case executions pass: 64 retained Clang cases, 152 generated
  checked cases, 64 generated cases with no-op witness hooks and no registry.
  Each body/profile also runs with an actual 1,048,576-byte stack limit. The same
  certificate/hash-verified object bytes are linked into both witness variants.
- Runtime negatives cover missing source/result/snapshot registration, reordered
  homes, copied identity with matching physical layout, changed signature,
  wrong live Frame, replayed epoch, before/after-safepoint failure, incorrect
  publication, quotas, first-failure preservation, same-layout scalar substitution
  and wrong extent. Successful empty outputs are covered in both witness modes.
- 48 protocol mutants reject with intact output sentinels: missing caller/callee
  entry/leave, redirected snapshot, missing publication, incorrect envelope
  mapping, post-pipeline removal of required operations or handoff, and an
  entered exit bypassing cleanup. Eight admission controls also reject. No
  rejected object is linked or executed.
- The existing full storage-auditor fixture/negative set passes unchanged. The
  ten-pointer regression retains four incoming-ownership rejections in each
  profile, zero incoming credit and intact sentinels. Raw Clang profile mismatch
  remains a separate negative; compiler-generated input needs no attribute repair.
- Five rebuilt affected suites pass, 5/5 in 11.28 seconds locally: gc_buffers,
  arrays_foundation, runtime_generated, runtime_failures and runtime_invariants.
  Existing failure sweeps and fatal controls remain enabled.
- Strict ASan/UBSan and leak detection pass 140 cases: 32 retained Clang,
  76 generated checked, 32 generated no-witness. Runtime/helpers/witnesses are
  instrumented; the exact certified boundary objects are not. This is not full
  generated-code sanitizer instrumentation. LeakSanitizer needs process inspection
  outside this host's sandbox; the permitted unrestricted run retained
  detect_leaks=1:halt_on_error=1:exitcode=87 and
  halt_on_error=1:print_stacktrace=1:exitcode=86, with no suppressions.

Ownership evidence now comes from generated construction/checks plus shared IR
validation; tests no longer authorize the callee. The no-witness executable has
no active-call registry or identity/root permission callback. The native Context
is still a trusted embedding contract. General native provenance, escaped/forged
bindings, recursive/concurrent calls and arbitrary caller IR are not approved.
Optimized store lineage, IR-to-final-call-target ownership correspondence, linked
helper/relocation semantics and complete final-executable ownership/failure
certification remain open. LLVM tracking handles and matching call/stack evidence
are not promoted into that missing proof. New composite allocation-failure
ordinals are not exhaustively swept; existing helper sweeps are retained.

Source changes, full/delta patches, certificates, LLVM, manifests and focused logs
are packaged in build/stage3b-ownership/generated-ownership-review.zip. No complete
six-configuration matrix, hosted CI, branches, commits, pushes or remote writes.
LANGUAGE_BIBLE.md and WRENCH_CLANKER.md were not edited; protected source hashes
also confirm unchanged storage-auditor/encoded-CFG/ten-pointer implementations.
Stop for owner and Wrench review before production integration or Stage 3C.

### Phase 23 Stage 3B — optimized ownership stores and linked code (2026-10-17)

Continued the current uncommitted generated-ownership worktree at accepted HEAD
7abc6249a5e1828fd49232a868dcc300b13b7894. Earlier pilot/integration/ownership
sources, results and ZIPs remain intact. The preceding ownership README is
preserved verbatim as tests/research/call_envelope/OWNERSHIP.md.

The new supplemental gate checks 88 original scalar store expressions after
normal/O2 optimization. It pins actual original LLVM object identities, checks
typed field projections and recursive loaded values, and requires each store to
reach its same-block Frame registration or pre-injection binding consumer without
an overlapping/unknown write or opaque call. Deleted/replaced original stores
reject even if a replacement appears equivalent. Caller null initialization
accepts only complete original aggregate zero or the observed exact zero memset.
The successful objects contain the same 88 scalar stores; O2 canonicalizes GEPs
and changes two 64-byte aggregate-zero initializations into two memsets. Machine
output in both profiles has eight movaps vector-store lines for initialization;
those observations are not ownership credits for packed stores. Existing result
publication, required-call arguments, safepoint dominance and cleanup
postdominance checks remain in force.

The new research-only linked gate consumes the actual lld map and retained ELF
relocations, after verifying the exact object against its machine-storage
certificate. With non-PIE/--emit-relocs/--no-relax it compares generated text
outside relocation operands, decodes all 57 direct calls, checks encoded targets
and retained external symbol/addend/type identity, matches the 52 generated
required-operation multiplicities, and checks local callee placement. Four
identity/root-offset references are checked against exact mapped compiler data
objects; original and linked signature/profile/extent contents agree. Eight
Runtime entry points are tied to the supplied archive members and corresponding
non-relocation body bytes. Executable, archive and member hashes are recorded.
This is local byte/target correspondence, not transitive helper or ownership
semantics. No candidate executes before successful linked delivery.

Verified normal/O2 results under pinned LLVM/Clang/lld 19.1.7:

- All four generated objects (return argument 0/3) are byte-identical to the prior
  ownership checkpoint. Each passes 88 scalar store facts, caller-null checking,
  52 required operations, unchanged storage auditing and final encoded CFG gates.
- Callee P=352, F=408/424, R=352, E=8, A=16, C=416/432 (normal/O2).
  Caller P=592, F=664, R=592, E=8, A=16, C=672 in both. No incoming homes or
  tail calls. Linked non-relocation text coverage is 3454/3448 bytes.
- Eight native linked variants pass: two profiles, two bodies and checked versus
  no-registry witnesses. Native totals: 64 C++ boundary, 152 generated checked,
  64 generated no-registry cases = 280, including 1 MiB stack repetitions.
- All original 48 protocol mutants retained; 32 new store/null-init controls
  produce 80 expected protocol rejections. All 56 linked mutations reject.
  Sentinels remain intact, and rejected artifacts are never executed.
- Full existing storage-auditor research controls pass unchanged. The old
  ten-pointer normal/O2 control still rejects four unproved incoming homes;
  its rejected output remains a sentinel.
- Focused gc_buffers, arrays_foundation, runtime_generated, runtime_failures and
  runtime_invariants: 5/5, 11.22 seconds, build/stage3b-linked/focused-tests.log.

Strict ASan/UBSan/LSan passes another 140 cases: 32 C++ boundary, 76 generated
checked and 32 generated no-registry. All eight generated sanitizer executables
also pass linked delivery. ASAN_OPTIONS=detect_leaks=1:halt_on_error=1:exitcode=87;
UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1:exitcode=86. C++ hooks and Runtime
are instrumented; certified LLVM objects remain exact and uninstrumented. Host
process inspection was allowed for strict LeakSanitizer; no leak/UB settings were
weakened. See build/stage3b-linked/sanitizers/complete.json.

Evidence is under build/stage3b-linked/accepted (source hashes, commands, original
and optimized LLVM, exact objects/certificates, linked maps/reports, mutation and
execution logs), trace.json (disassembly commands, object equality, IR forms and
frame figures) and sanitizers. Intermediate runs under review/verified/final are
not accepted final evidence: source-hash guards rejected runs whose source was
edited during gate hardening. The source-frozen accepted run completed normally.

The compile-only helper_alternative.cpp produces normal/O2 LLVM and machine
output for small noinline Slot/field initializers, using existing Runtime::Slot
and no registry. Its O2 output is short direct store sequences with no calls. It
is not linked or promoted into Runtime. A reviewed no-safepoint native helper
contract is the simplest conservative alternative to a broad x64 store-lineage
engine; helper argument identity and body semantics would still require proof.

Remaining obligations: no general IR-to-machine store lineage, machine register
or call-argument semantics, arbitrary data-relocation/dynamic-loader proof,
transitive Runtime/helper semantic proof or all-path final-byte ownership,
publication/failure/cleanup certificate. Both optimized_store_lineage_certified
and final_byte_ownership_certified remain false. Matching field values, stack
offsets, call counts and passing executions grant no ownership authority.

LANGUAGE_BIBLE.md, WRENCH_CLANKER.md, storage_audit.cpp, encoded_cfg.cpp and the
original storage_audit test remain hash-identical to sprint entry. No production
ABI activation, Stage 3C, new ownership registry, LLVM fork, production 64 KiB
policy, full six-configuration matrix, hosted CI, branch/commit/push or remote
write. Stop for owner and Wrench review before production integration.

Review artifacts: build/stage3b-linked/optimized-linked-review.zip, cumulative
optimized-linked-review.patch and incremental
optimized-linked-since-ownership.patch (against the preceding ownership ZIP's
source snapshot). Both patches are application-checked and reconstructed sources
must match the final scoped files. Protected documents are excluded.

## Order 1: optional LLVM certification, 17 October 2026

Implemented against the current local worktree, preserving its pre-existing
private-envelope/runtime experiments. No prior Order 1 ZIP was used. The
independent review's recorded conclusions in WRENCH_CLANKER.md and the owner's
clarification supplied the scope: machine identity and final-byte certification
only. No Order 2 or source-array implementation.

Findings and changes:

- Ordinary JIT already called lower/finish/ORC without an ownership inventory.
  Ordinary AOT already called the same lowerer, LLVM verification/O2 and
  TargetMachine object emission without freeze/deliver certification. There was
  no demonstrated production acceptance defect to bypass.
- The remaining coupling was at build/test level: storage_audit.cpp and
  encoded_cfg.cpp were unconditional compiler-library inputs. They and the
  private research entrypoints/inventory instrumentation now require the
  default-OFF CMake option VOID_LLVM_RESEARCH. The research-only disassembler
  dependency is requested with that option. The default library contains only
  backend.cpp.o, with no defined research-emitter/certification symbols.
- The existing arrays_storage_audit suite is retained under that option and
  labeled research. Its invalid-fixture, lineage, incoming-home, encoded-CFG
  and final-byte negative controls remain intact. Explicit --storage-proof
  requests in test harnesses report the missing build option when disabled;
  they do not silently fall back to ordinary emission.
- Added paired acceptance regressions: the same aggregate main/numeric programs
  rejected for unproved incoming ownership still emit normally and execute in
  JIT, normal/O2. The inheritance stress program rejected for final encoded
  calls still emits normally and executes optimized JIT. These are unsupported
  research-checker shapes, not evidence of LLVM miscompilation. Existing CLI
  tests execute the aggregate objects after native linking.
- Typed IR validation, LLVM verification, precise Frame/Slot construction,
  safepoints/root lifetimes, independent value snapshots, transactional
  publication, cleanup and conservative runtime call memory effects are
  unchanged. LLVM 19.1.7 and the shared lowerer/runtime remain in use. The
  historical 64 KiB research budget is confined to optional experiments; no
  production limit was added or relaxed.

Local Linux validation (LLVM/Clang/LLD 19.1.7; Debug):

```
cmake -S . -B build/order1/production -DCMAKE_BUILD_TYPE=Debug \
  -DCMAKE_C_COMPILER=/usr/bin/clang-19 -DCMAKE_CXX_COMPILER=/usr/bin/clang++-19 \
  -DLLVM_DIR=/usr/lib/llvm-19/lib/cmake/llvm \
  -DVOID_LINKER=/usr/lib/llvm-19/bin/ld.lld \
  -DVOID_CLANG=/usr/lib/llvm-19/bin/clang-19 \
  -DVOID_DWARFDUMP=/usr/lib/llvm-19/bin/llvm-dwarfdump
cmake --build build/order1/production --target voidc void_tests void_managed_tests void_runtime_generated void_runtime_aot_normal void_runtime_aot_optimized void_gc_buffers -j 4
cmake --build build/order1/production --target void_inheritance_cli_stress_normal void_inheritance_cli_stress_optimized -j 2
ctest --test-dir build/order1/production -R '^(core|jit|gc|native|value_structs_cli|managed_source|runtime_generated|runtime_aot_normal|runtime_aot_optimized|gc_buffers)$' --output-on-failure -j 2
cmake -S . -B build/ci-debug -DVOID_LLVM_RESEARCH=ON
cmake --build build/ci-debug --target void_arrays_storage_audit voidc -j 3
cmake --build build/ci-debug --target void_arrays_storage_audit -j 3
ctest --test-dir build/ci-debug -R '^arrays_storage_audit$' --output-on-failure
ctest --test-dir build/ci-debug -R '^value_structs_cli$' --output-on-failure
```

All builds succeeded. Production focused CTest: 10/10 passed (9.86 seconds).
Research suite: 1/1 passed (8.95 seconds), including new acceptance regressions
and preserved negative controls. Research-enabled value_structs_cli: 1/1 passed
(1.90 seconds), exercising linked AOT and JIT with/without O2. Additional direct
runs of inheritance/stress.void at inputs -1, 0, 10 passed JIT and linked AOT in
both modes (12 executions; expected input + 1100). A disabled research request
failed with the expected VOID_LLVM_RESEARCH=ON diagnostic and preserved an
existing output sentinel. llvm-ar/llvm-nm and CTest inventory inspection confirmed
research code/test absence from the default build. Logs are in build/order1/.
Changed implementation files pass git diff --check; existing trailing Markdown
spaces in WRENCH_CLANKER.md were left intact.

Official LLVM references consulted: [LLVM 19 code generator](https://releases.llvm.org/19.1.0/docs/CodeGenerator.html)
and [LLVM 19 language reference](https://releases.llvm.org/19.1.0/docs/LangRef.html).
LLVM's optimizer/code generator owns instruction selection, ABI lowering,
register allocation and encoding; VOID still owns correct input IR and runtime
contracts. No LLVM correctness failure was demonstrated in this order.

Limitations: Linux Debug focused execution only; no new Release, sanitizer,
Windows, macOS or hosted-CI claim. Research limitations deliberately remain in
opt-in experiments. Source arrays remain unsupported natively. No commits,
pushes, branches or remote writes were performed.

## Order 2: simplify private fixed-array calls, 17 October 2026

Completed against the current worktree, retaining Order 1. Inspection confirmed
that the Envelope/CallBinding implementation was an optional closed research
fixture, not production source-array lowering. No source-array implementation,
new backend, ownership registry, certification framework or Git mutation was
introduced. Pre-edit files are retained locally in build/order2/baseline/.

Changes:

- Replaced Envelope and CallBinding with direct LLVM parameters: eight argument
  home pointers plus result, layout, quota and failure. The callee is internal;
  LLVM's normal calling convention handles native register/stack transport.
  The test callee remains noinline to exercise an actual native call.
- Removed duplicated argument/context metadata, identity/profile/epoch checks,
  expected-Frame root authorization and repeated explicit root-validation calls.
  Removed the 330-line EnvelopeProtocol verifier, synthetic ownership-wrapper
  validation, mutation protocol, linked-byte/ABI reconstruction gate and duplicate
  C++ envelope/helper alternatives. Historical research reports are marked as
  superseded, with their historical claims preserved.
- Kept one static scalar layout/extent check at the embedding boundary. The
  existing Runtime helpers remain responsible for registered roots, valid/live
  buffer layouts, independent copies, polling/leases and transactional output.
  Removed only the research-specific expected-Frame overload of root_word;
  its normal exact live-slot registration checks and expired-token checks remain.
- Caller snapshots are independent Runtime buffers with ordered source effects.
  Caller roots remain live across the private call. The callee loads the rooted
  incoming handles into its own precise Frame/Slots before collecting, and
  retains them through result copying. Both frames are removed on failure and
  success. Payload size does not enlarge native homes; no 64 KiB cap is imposed.
- Reused the shared lowerer, LLVM verifier/O2 pipeline, native TargetMachine
  factory and ordinary object writer. The private fixture no longer invokes the
  research storage auditor or requests byte certificates. Order 1's default-OFF
  VOID_LLVM_RESEARCH option and independent auditor/negative controls remain.
- Added normal/O2 CTest behavior tests, using the identical generated object
  with observing hooks and empty hooks. Hooks grant no permission and provide no
  call metadata. Deleted duplicate no-witness source in favor of one test source
  compiled in both modes. The small Python driver now only builds/runs CTest.

Focused Linux Debug validation, LLVM/Clang/LLD 19.1.7:

```
cmake -S . -B build/ci-debug -DVOID_LLVM_RESEARCH=ON
cmake --build build/ci-debug --target void_private_fixed_call_normal_0_2_observed -j 3
ctest --test-dir build/ci-debug -R '^private_fixed_call_normal_0_2_observed$' --output-on-failure
python3 tests/research/call_envelope/run.py --build build/ci-debug
cmake --build build/ci-debug --target void_arrays_storage_audit -j 2
ctest --test-dir build/ci-debug -R '^arrays_storage_audit$' --output-on-failure
cmake --build build/order1/production --target voidc void_gc_buffers void_runtime_generated void_runtime_aot_normal void_runtime_aot_optimized void_managed_tests -j 2
ctest --test-dir build/order1/production -R '^(gc_buffers|runtime_generated|runtime_aot_normal|runtime_aot_optimized|managed_source|value_structs_cli)$' --output-on-failure -j 2
```

All builds and tests passed. The private-call driver builds/runs the 12 targets
`void_private_fixed_call_{normal,optimized}_{0_2,3_2,7_16384}_{observed,unobserved}`
with CTest label private_calls. Final run: 12/12 passed in 6.17 seconds, totaling
162 behavioral cases (six observed executables with 15 cases each, six empty-hook
executables with 12 cases each). Tests check snapshot order/payload independence,
argument transport, roots across collection, expired caller/callee tokens,
independent complete results, unregistered source/result, source/result alias,
preserved existing failure, invalid quotas/layout/type/extent, absent layout or
quota, null source, initially null result, pre/post-poll failures, failed return
publication and zero retained temporary buffers after collection. The selected
arguments are 0 and 3 at extent 2, and 7 at extent 16384 (128 KiB). Each of six
object generations also rejects three invalid typed/body inputs with an existing
output sentinel intact and confirms ordinary source-array native rejection.

Additional stack check: directly executed
`build/ci-debug/void_private_fixed_call_{normal,optimized}_7_16384_{observed,unobserved} 7 16384`
with subprocess RLIMIT_STACK soft/hard both 1048576 bytes. All four executables
passed (54 additional cases), demonstrating large heap payloads with a 1 MiB
native stack. Command/output log: build/order2/small-stack-tests.log.

Affected production tests passed 6/6 in 9.91 seconds, including existing Runtime
allocation-failure ordinals, copy/publication rollback, GC coordination, generated
roots, ordinary normal/O2 linked AOT and source JIT behavior. The separate Order 1
auditor passed 1/1 in 8.99 seconds, including its deliberate negative controls.
llvm-ar/llvm-nm confirmed the default production archive still contains only
backend.cpp.o and no private-call/research-emitter certificate symbols.
Implementation files pass git diff --check; pre-existing Markdown hard-break
spaces in WRENCH_CLANKER.md remain unchanged. Logs are in build/order2/.

Official references consulted before implementation: [LLVM 19 calling conventions](https://releases.llvm.org/19.1.0/docs/LangRef.html#calling-conventions)
and [LLVM 19 code generation](https://releases.llvm.org/19.1.0/docs/CodeGenerator.html).
No independent reconstruction of register/stack argument transport is required
by this fixture.

Limitations: this remains a closed scalar-u64, nonempty fixed-array research
fixture with eight arguments and an argument-return body. No general private-call
or source-array native lowering was added; ordinary source arrays still reject.
The new fixture was executed through linked AOT only, not a private-array ORC
entry. Ordinary JIT was covered by existing managed/aggregate tests. Windows,
macOS, Release, sanitizers and full/hosted CI were not run. No genuine production
correctness failure was observed. No commits, pushes, branches or remote changes.


## Order 3: real source numeric fixed arrays, 17 October 2026

Implemented the production feature against the current Orders 1/2 worktree.
The existing frontend already parsed/typed fixed arrays and produced validated
fixed_array/array_get/array_set IR; ordinary LLVM lowering was the missing piece.
It is now enabled in default builds, without invoking any research emitter.

Implementation:

- Represent numeric/bool fixed values as handles to existing Runtime buffers.
  Add handle offsets to generated Frame/Slot and managed/value-field layouts;
  payload bytes never become native array allocas. The existing shared lowerer
  serves JIT and AOT, with normal LLVM pointer/scalar argument/return lowering.
- Lower literals/defaults, exact-width checked indexing, functional updates and
  snapshots. Reuse existing frontend owner/index/RHS sequencing, including old
  scalar snapshots for compounds and reacquisition after RHS field mutations.
- Eagerly copy array storage at local/snapshot stores, parameter initialization,
  managed-field writes and returns. Recursively copy array fields inside value
  aggregates into rooted staging, publishing the complete value after success.
  No copy-on-write or const-borrowing contract was introduced. Repeated defaults
  remain compact IR; empty arrays remain typed null handles internally.
- Four small source ABI helpers use Runtime's admitted scalar schemas, existing
  buffer constructors, leases, copies, scalar access, quotas and failure channel.
  Scalar schemas are admitted through the existing metadata owner; no separate
  ownership registry, allocator or collector was added. The internal byte quota
  is a polling chunk size, not an array-size limit.
- Integers retain their exact bits and signedness; floats use bit transport,
  including NaN and negative zero. Signed indexes normalize without negating MIN;
  unsigned indexes are checked before narrowing. Invalid accesses return source
  diagnostics and leave generated frames normally.
- Preserve Order 1's default-OFF research option and its negative certificates;
  preserve Order 2's direct-call fixture. Update the former source-array rejection
  test to require ordinary successful emission, and remove the obsolete research
  fixture assertion that all source arrays must reject.

Added eleven actual .void examples under examples/fixed_arrays/: the requested
sample, independent copies/eight-argument calls/returns, selected-write and
literal/argument evaluation order, all scalar encodings, signed/unsigned index
extremes, empty arrays, 2 MiB payloads, value/managed fields, bounds-before-RHS
failure, loops and short-circuit expressions. The sample returns 17 for input 7.
See docs/FIXED_ARRAYS.md for behavior and remaining scope.

Exact focused commands (configured LLVM/Clang/LLD 19.1.7, Linux Debug):

```
cmake --build build/order1/production --target voidc -j 3
cmake -S . -B build/order1/production
cmake --build build/order1/production --target void_fixed_arrays_tests -j 3
cmake --build build/order1/production --target void_fixed_arrays_cli_main_normal void_fixed_arrays_cli_copies_normal void_fixed_arrays_cli_order_normal void_fixed_arrays_cli_numeric_normal void_fixed_arrays_cli_indexes_normal void_fixed_arrays_cli_index_types_normal void_fixed_arrays_cli_empty_normal void_fixed_arrays_cli_large_normal void_fixed_arrays_cli_value_fields_normal void_fixed_arrays_cli_bounds_order_normal void_fixed_arrays_cli_loops_normal void_fixed_arrays_cli_main_optimized void_fixed_arrays_cli_copies_optimized void_fixed_arrays_cli_order_optimized void_fixed_arrays_cli_numeric_optimized void_fixed_arrays_cli_indexes_optimized void_fixed_arrays_cli_index_types_optimized void_fixed_arrays_cli_empty_optimized void_fixed_arrays_cli_large_optimized void_fixed_arrays_cli_value_fields_optimized void_fixed_arrays_cli_bounds_order_optimized void_fixed_arrays_cli_loops_optimized -j 3
cmake --build build/order1/production --target void_fixed_arrays_tests void_arrays_foundation void_gc_buffers void_managed_tests void_runtime_generated void_runtime_aot_normal void_runtime_aot_optimized -j 3
ctest --test-dir build/order1/production -R '^(fixed_arrays_source|fixed_arrays_cli|arrays_foundation|gc_buffers|managed_source|runtime_generated|runtime_aot_normal|runtime_aot_optimized|value_structs_cli)$' --output-on-failure -j 2
ctest --test-dir build/order1/production -R '^fixed_arrays_(source|cli)$' --output-on-failure
cmake -S . -B build/ci-debug -DVOID_LLVM_RESEARCH=ON
cmake --build build/ci-debug --target void_arrays_storage_audit void_private_fixed_call_normal_0_2_observed void_private_fixed_call_optimized_7_16384_unobserved -j 3
ctest --test-dir build/ci-debug -R '^(arrays_storage_audit|private_fixed_call_normal_0_2_observed|private_fixed_call_optimized_7_16384_unobserved)$' --output-on-failure
```

Results:

- Focused production/dependency suites: 9/9 passed, 16.51 seconds. This includes
  analysis/IR negatives, Runtime buffers and existing allocation-failure controls,
  managed source/rooting, generated safepoints, normal/O2 runtime AOT and ordinary
  value-struct CLI execution.
- Final array-only recheck after adding constant-template and floating-bit
  examples: 2/2 passed, 11.39 seconds (source 4.35; CLI 7.04). The CLI suite runs
  116 normal/O2 JIT/linked-AOT executions, including 24 expected checked bounds
  failures. The source harness uses Runtime allocation budget 1 and inspects
  actual registered roots, then verifies no call temporaries survive collection.
- Source allocation injection: 327 failing budgets in normal mode and 327 in O2,
  every failure followed by collection and a successful retry. Additional direct
  production-ABI allocation sweeps keep the old result sentinel intact on every
  failed copy and verify two rooted objects only, then zero after clearing roots.
- Orders 1/2 focused regressions: 3/3 passed, 10.03 seconds, including the full
  optional storage-auditor suite and representative normal/optimized direct-call
  fixtures. Research-only invalid fixtures remain intentional rejections.
- Four large-source runs passed under RLIMIT_STACK soft/hard 1048576: ordinary
  voidc run large.void 7, normal/O2, and linked native large normal/O2 executables.
  Each returned 53 while manipulating independent 2 MiB arrays. Exact argv and
  output are in build/order3/small-stack-tests.log.
- llvm-ar/llvm-nm confirm the default backend archive still contains only
  backend.cpp.o, with no research emitter/certification entrypoints. Changed code
  and new sources pass whitespace checks; pre-existing Markdown hard-break
  spaces in WRENCH_CLANKER.md remain intact.

During development the new target initially required CMake regeneration. One
source-test run used the stale 155 expectation after the order fixture gained
an extra literal-evaluation side effect; updating/rebuilding its expectation to
156 resolved that test mismatch. Final source/CLI runs pass. No LLVM or Runtime
correctness failure was demonstrated or hidden by those test corrections.

Logs and exact expanded AOT build command are in build/order3/. Pre-edit sources
are in build/order3/baseline/; implementation.diff isolates the production changes
against the actual local Orders 1/2 source. These are local development artifacts,
not a Git commit or hosted result.

LLVM references consulted: [LLVM 19 calling conventions](https://releases.llvm.org/19.1.0/docs/LangRef.html#calling-conventions)
and [LLVM 19 bitcast](https://releases.llvm.org/19.1.0/docs/LangRef.html#bitcast-to-instruction).

Remaining scope/limits: numeric/bool scalar fixed elements only; nested,
struct/string/reference elements and growable arrays remain unsupported natively.
Array-specific traversal/ranges and .count are not implemented by this order.
Copies are deliberately eager and can be redundant; instruction/staging roots
can retain temporaries until function exit. Const-formal borrowing and performance
optimization are deferred. Windows, macOS, Release, sanitizers and full/hosted CI
were not run. Orders 1/2 remain in place. No Order 4 work, commits, pushes,
branches or remote modifications were performed.

## 2026-10-17 — Order 4: measured root storage and Runtime lookup efficiency

Inspected the actual uncommitted Orders 1-3 implementation, FIXED_ARRAYS.md,
WRENCH_CLANKER.md and the latest work-log entries before editing. Those changes
remain intact. This order changes only Runtime lookup/publication fast paths and
focused tests/documentation. No Order 5, LLVM/backend/frontend change, collector
rewrite, Git mutation or remote modification.

Baseline and method:

- Saved pre-edit runtime sources and headers in build/order4/baseline/; saved a
  Release runtime archive and compiler before production changes. Configured
  build/order4/release with clang/LLVM/LLD 19.1.7, Release (-O3 -DNDEBUG), research
  disabled. Host: Linux x86-64, Ryzen 7 1700X. No sanitizer or LTO.
- The small manual benchmark tests/runtime/root_lookup_measure.cpp measures only
  the paths investigated here. Five timed batches after warmup; median/min/max
  ns/op in CSV. Separate 100-operation allocation counts intercept C++ new,
  including aligned new. Counts are requested allocation bytes, not RSS or peak
  live storage. No timing assertions or general benchmarking framework added.
- Baseline measurements preceded changes: root-token lookup doubled its search,
  1,024-root source reads cost about 4.55 us, scalar layout creation allocated
  three times/103 bytes, and empty publication cost about 9.86 us with 1,024
  buffers despite just two stack roots. Source lookup and heap alias scanning
  were isolated in separate cases. Initial/raw measurements remain under
  build/order4/baseline/measure.csv and initial-source-timings.json.
- Final baseline-versus-updated comparison ran serially, without concurrent
  builds/tests, pinned to CPU 0. Baseline benchmark uses the saved baseline
  runtime.hpp AND heap.hpp: mixing headers with different Heap sizes would be
  invalid. Final benchmark uses current headers. Source AOT comparisons link the
  SAME baseline-generated object against each matching Runtime archive. No code
  generation difference is included in the Runtime comparison.

Findings and changes:

1. Frame entry validates and links caller-owned storage; it performs zero heap
   allocations at 8, 128 and 1,024 slots. Compiler root homes are allocated once
   per static instruction/local/staging type and reused through loop iterations,
   not allocated per dynamic array access. Frame exit unlinks without allocation.
   Temporary retention until function exit remains bounded by these static homes.
   No measured root-storage allocation problem justified a new root allocator or
   liveness analysis, so compiler storage/lifetime behavior is unchanged.
2. require_output used to scan every root even after a match; root_word then
   repeated the search. require_output/require_buffer_output now return the
   matching Frame and root_word reuses it. reference_source also stops its exact
   originating-frame membership search at a match. Runtime/thread ownership,
   originating-frame linkage, exact slot membership and private-root exclusions
   remain checked. Nested duplicate registrations do not invalidate older tokens.
3. Source read/write/copy previously allocated a new shared capacity layout every
   time. They now recover the source buffer's existing immutable layout after
   checking canonical catalog schema, default packing, full live extent and
   publication. Stack-constructed expected geometry uses the existing
   BufferLayout implementation, avoiding a second geometry implementation. The
   existing scalar-schema admission logic is shared by both factory paths.
   Empty arrays retain the factory path. Copies still allocate independent data;
   only immutable metadata is shared. Default creation still uses the factory.
4. Heap::validate_output_address scanned the entire object table whenever any
   buffer existed. Two conservative inclusive address bounds on the existing
   Heap skip the scan for outputs definitely outside buffer storage. Successful
   adoption widens them; a first buffer resets them. Deletion need not shrink
   them: false positives run the original full scan. Saturating upper-bound
   arithmetic and subtraction for lower-edge pointer overlap avoid wraparound.
   Potential aliases retain the original BufferBusy/invalid-output checks.
   Updates occur under the existing Runtime mutex; direct Heap remains its
   existing single-threaded interface. No per-object index/global cache added.
5. Allocation/type/buffer/reference lookup otherwise uses the existing object
   hash table and admitted catalogs. Mixed heaps and buffer-only heaps had the
   same read scaling, identifying root traversal rather than payload-table size
   as the read bottleneck. Copy/publish scans were separately demonstrated above.
   Independent payload copies, read leases and GC graph traversal remain ordinary
   required work. Eager compiler copies were not rewritten speculatively.

Pinned final microbenchmark medians (ns/op):

| Workload | Baseline | Updated |
| --- | ---: | ---: |
| Frame enter/leave, 1,024 slots | 1,509 | 1,505 |
| Create root token, last of 1,024 slots | 2,384 | 1,082 |
| Source read, 1 buffer / 2 slots | 314 | 282 |
| Source read, 1,024 buffers / 1,025 slots, source first | 4,148 | 282 |
| Source read, 1 buffer + 1,024 ordinary objects/roots | 4,164 | 284 |
| Empty publication, 1,024 buffers / 2 stack roots | 9,927 | 30.6 |
| Copy + collect, 1,024 retained buffers/roots | 51,190 | 46,219 |
| Copy + collect, 1 MiB payload / 2 roots | 58,360 | 59,270 |
| Copy + collect, 2 MiB payload / 2 roots | 157,790 | 150,950 |

A source read now allocates once/31 bytes (schema-key string) instead of three
allocations/103 bytes. Copy+collect with two roots allocates 9 times versus 11;
requested bytes are payload+263 versus payload+335, at BOTH 1 MiB and 2 MiB.
That includes transient builder/collection bookkeeping, not just retained object
metadata. There is no payload-proportional metadata growth in this measurement.
Large copies show no reliable throughput change; preserving independent data is
more important than the small fixed metadata saving. No claim of universal speedup.

Real source AOT: CPU 0, two warmup pairs, 15 alternating before/after samples per
case; medians include process startup. Every execution's answer was checked.
access.void reads a 16-element i32 array 200,000 times (answer 1,700,000);
mutate.void updates element zero 5,000 times (answer 4,999). Programs are saved
in build/order4/. large is the existing examples/fixed_arrays/large.void, input 7,
answer 53, using independent 2 MiB arrays. Existing entry Runtime budget 256 stays
unchanged, so the mutation workload includes ordinary automatic collection.

| Source program | Baseline ms normal / O2 | Updated ms normal / O2 |
| --- | ---: | ---: |
| access | 74.391 / 78.635 | 72.733 / 69.978 |
| mutate | 62.524 / 62.681 | 42.044 / 41.541 |
| existing large | 17.613 / 17.734 | 17.690 / 17.837 |

The initial unpinned large-program samples were noisy (including an apparent O2
regression); CPU-pinned paired measurements resolved this to less than 1% with
overlapping ranges. Both raw datasets are retained. The tiny normal access gain
also has overlapping ranges; the mutation gain is clear in both modes. These
measurements justify the specific removed work, not additional optimizations.

Exact measurement commands (from repository root):

```sh
cmake -S . -B build/order4/release -DCMAKE_BUILD_TYPE=Release -DCMAKE_C_COMPILER=/usr/bin/clang-19 -DCMAKE_CXX_COMPILER=/usr/bin/clang++-19 -DLLVM_DIR=/usr/lib/llvm-19/lib/cmake/llvm -DVOID_LINKER=/usr/lib/llvm-19/bin/ld.lld -DVOID_CLANG=/usr/lib/llvm-19/bin/clang-19 -DVOID_DWARFDUMP=/usr/lib/llvm-19/bin/llvm-dwarfdump
cmake --build build/order4/release --target voidc void_gc_buffers -j 3
/usr/bin/clang++-19 -std=c++20 -O2 -Ibuild/order4/baseline/include -Iinclude tests/runtime/root_lookup_measure.cpp build/order4/baseline/libvoid_runtime.a -pthread --ld-path=/usr/lib/llvm-19/bin/ld.lld -o build/order4/baseline/measure
/usr/bin/clang++-19 -std=c++20 -O2 -Iinclude tests/runtime/root_lookup_measure.cpp build/order4/release/libvoid_runtime.a -pthread --ld-path=/usr/lib/llvm-19/bin/ld.lld -o build/order4/final/measure
taskset -c 0 build/order4/baseline/measure > build/order4/baseline/pinned-measure.csv
taskset -c 0 build/order4/final/measure > build/order4/final/pinned-measure.csv
python3 build/order4/build_measurements.py
python3 build/order4/run_measurements.py
```

The two small local scripts save the expanded emit/link argv in
aot-commands.json and individual process timings in source-timings.json. They
are local development artifacts, as are saved baseline headers/archive/compiler;
the focused C++ measurement source is retained in tests/runtime/ for rerunning
against matching headers/archive from any revision.

Focused correctness verification, exact commands:

```sh
cmake --build build/order1/production --target void_gc_buffers void_runtime_tests void_fixed_arrays_tests void_managed_tests void_runtime_generated void_runtime_aot_normal void_runtime_aot_optimized -j 3
python3 - <<'PY'
import subprocess
from pathlib import Path
targets = ['void_fixed_arrays_cli_' + p.stem + '_' + mode
           for mode in ['normal', 'optimized']
           for p in Path('examples/fixed_arrays').glob('*.void')]
subprocess.run(['cmake', '--build', 'build/order1/production', '--target', *targets, '-j', '3'], check=True)
PY
ctest --test-dir build/order1/production -R '^(fixed_arrays_source|fixed_arrays_cli|gc_buffers|runtime_coordination|managed_source|runtime_generated|runtime_aot_normal|runtime_aot_optimized)$' --output-on-failure -j 2
ctest --test-dir build/order4/release -R '^gc_buffers$' --output-on-failure
```

Results: Debug 8/8 passed in 13.74 s; Release Runtime buffers 1/1 passed in
0.83 s. Source tests passed 215 failing allocation budgets in EACH normal/O2
mode, with collection and successful retry after every failure; transactional
ABI failure sweeps also passed. CLI runs all 116 existing real-source normal/O2
JIT/AOT cases, including expected bounds failures and 2 MiB payloads. Runtime
buffer tests include allocation/adoption/partial-copy/collection failure
injection, genuine concurrent collection, safepoint leases and aborted builders.
New regressions cover matching-layout pointer reuse, wrong encoding/extent,
custom schema, padded packing, incomplete extent, empty fallback, independent
payloads, nested duplicate roots, changed registration and expired inner tokens.
Alias tests cover first/last byte and pointer-sized overlap before a buffer,
leased and unleased, across collection to an empty buffer heap and re-adoption.
All existing alias and allocation cleanup cases remain enabled. Logs are
build/order4/{focused-tests,release-tests}.log and corresponding build logs.

Remaining limitations: root search remains linear in the worst case; possible
buffer aliases still scan the object table, and conservative stale bounds can
increase false positives. A schema-key string allocation remains per source
operation; default/empty layouts still allocate. Compiler roots retain some
instruction temporaries until function exit; eager copies remain. No evidence
here justified a registry/cache, root-liveness rewrite or copy-elision project.
Timings are local single-host observations; cross-platform/full CI and sanitizer
runs were not performed. No production size ceiling was introduced. Orders 1-3
are preserved; Order 5 was not started.

## 2026-10-17 — Order 5: simplify source-private LLVM function linkage

Audited the actual current Orders 1-4 worktree, backend construction/emission,
frontend ownership/visibility, JIT bindings/lookups, reload leases, imports,
callbacks, metadata and existing module/native tests. Saved the pre-edit backend,
module tests, CMake and compiler in build/order5/baseline/. No Runtime, frontend,
calling-convention, symbol-name, GC or array implementation changes in this order.

Official LLVM references consulted first:
[LLVM 19 linkage types](https://releases.llvm.org/19.1.0/docs/LangRef.html#linkage-types),
[ORC lookup, mangling and resource tracking](https://releases.llvm.org/19.1.0/docs/ORCv2.html),
and [DISubprogram flags](https://releases.llvm.org/19.1.0/docs/LangRef.html#disubprogram).
Internal linkage permits local object symbols; it does not require removing
function names from debug information. Existing LLJIT lookup, MangleAndInterner
and ResourceTracker remain the implementation for external resolution/lifetimes.

Findings and exact production changes:

- lower_graph combines the complete source import graph into ONE typed IR/LLVM
  module. File-private functions already carry module_identity and visibility;
  their definitions nevertheless all used ExternalLinkage. No cross-object
  native consumer is entitled to those unselected file-private implementations.
- The shared lowerer now uses InternalLinkage only for source-owned functions
  with visibility == private, unless that function is the exported entry.
  This one rule is applied at function construction. Jit::compile passes its
  actual selected function identity, which may differ from module.entry_symbol;
  AOT emission passes module.entry_symbol. The optional research caller supplies
  its entry too. There is no internalization pass or extra export registry.
- DISubprogram flags now include SPFlagLocalToUnit when the resulting function
  has local linkage. Logical names, native names, source paths/locations, function
  signatures, call lookup maps and generation suffixes are unchanged.
- Public and project/internal-visible source functions remain external, including
  imported source exports. Legacy hand-built IR functions remain external: they
  are used by separately linked native harnesses and can be imported by another
  LLVM module. This intentionally does not maximize hidden symbols.
- Trusted C import declarations remain external. Runtime ABI declarations,
  callback bindings, main, root-probe host entry, selected JIT entry, nominal and
  interface metadata globals remain as before. Metadata is consumed by native
  bootstrap/harnesses, so it was not internalized. Generated virtual dispatch and
  interface prepare/invoke helpers were already internal and needed no change.

Evidence from actual ELF objects, using the same private-array module fixture
with the pre-edit compiler versus the updated lowerer:

| Function | Before normal / O2 | After normal / O2 |
| --- | --- | --- |
| /main.void::helper (private scalar) | global / global | local / inlined away |
| /lib.void::snapshot (private array argument/return) | global / global | local / local |
| /main.void::entry | global / global | global / global |
| /lib.void::imported (public export) | global / global | global / global |
| /lib.void::project (project-visible export) | global / global | global / global |

Both CLI objects (existing __void suffix) and library objects show the intended
bindings. The automated modules test reads native object symbol flags through
LLVM's existing ObjectFile API: private definitions cannot be global, normal
private bodies must exist, and required external exports must exist in both
modes. O2 may remove unused/inlined private bodies. Platform global-name prefixes
are allowed. CMake gives only the existing void_tests target LLVM header access;
no new compiler API, verification service or certification system is involved.

New focused regressions also exercise:

- Private scalar and array bodies across source-file imports, array argument
  snapshots, result publication and copy independence.
- Explicit JIT selection of a private non-default entry, and selection of an
  imported public function, normal/O2.
- Replacing a private helper body while an old generation lease remains usable;
  forced Runtime allocation budget 1, post-return collection and no live roots.
- A separate LLVM consumer module resolving the provider's unselected legacy
  calculate__v1 export through ORC, normal/O2. The provider lease stays alive,
  preserving the existing host-owned dependency lifetime contract.

Exact focused commands, using the pinned LLVM 19.1.7 Debug build:

```sh
/usr/lib/llvm-19/bin/llvm-config --version
cmake --build build/order1/production --target void_tests voidc void_runtime_callbacks void_runtime_generated void_runtime_aot_normal void_runtime_aot_optimized void_fixed_arrays_tests -j 3
cmake --build build/order1/production --target void_tests void_inheritance_tests void_inheritance_aot_normal void_inheritance_aot_optimized void_interfaces_tests -j 3
ctest --test-dir build/order1/production -R '^(modules|jit|reload|native|modules_cli|runtime_callbacks|runtime_generated|runtime_aot_normal|runtime_aot_optimized|fixed_arrays_source|gc_buffers)$' --output-on-failure -j 2
python3 - <<'PY'
from pathlib import Path
import subprocess
targets = ['void_fixed_arrays_cli_' + p.stem + '_' + mode
           for mode in ['normal', 'optimized']
           for p in Path('examples/fixed_arrays').glob('*.void')]
subprocess.run(['cmake', '--build', 'build/order1/production', '--target', *targets, '-j', '3'], check=True)
PY
ctest --test-dir build/order1/production -R '^(fixed_arrays_cli|inheritance_source|interfaces_source|inheritance_aot_normal|inheritance_aot_optimized)$' --output-on-failure -j 2
cmake --build build/ci-debug --target void_arrays_storage_audit void_private_fixed_call_normal_0_2_observed void_private_fixed_call_optimized_7_16384_unobserved -j 3
ctest --test-dir build/ci-debug -R '^(arrays_storage_audit|private_fixed_call_normal_0_2_observed|private_fixed_call_optimized_7_16384_unobserved)$' --output-on-failure
python3 build/order5/check_objects.py
/usr/lib/llvm-19/bin/llvm-ar t build/order1/production/libvoid_llvm.a
/usr/lib/llvm-19/bin/llvm-nm --defined-only build/order1/production/libvoid_llvm.a
```

Results:

- Production: 11/11 passed, 66.09 s; additional dispatch/array suites 5/5 passed,
  10.00 s. These are targeted suites, not the complete compiler matrix.
- Orders 1/2 research regressions: 3/3 passed, 10.30 s. The default archive still
  contains only backend.cpp.o and no research emitter/certification entrypoints.
- Fixed-array CLI: all 116 normal/O2 JIT/linked-AOT cases pass, including checked
  failures and 2 MiB arrays. Source allocation failures remain 215 failing
  budgets per normal/O2 mode, with collection, cleanup and successful retries.
  gc_buffers retains the Order 4 layout reuse, alias-boundary, root lifetime,
  collection and allocation-failure regressions. Runtime source files were not
  edited by Order 5.
- New fixture native checks: four successful CLI/library executions, six actual
  object symbol dumps and six llvm-dwarfdump --verify checks passed. The library
  harness separately links against the PUBLIC/imported and project-visible
  generated functions and external nominal/witness catalogs, forces GC during
  private array calls and confirms no remaining objects after return.
- The local check_objects.py script records every emit/link/nm/dwarfdump argv in
  build/order5/object-commands.json. Inputs/objects are generated by the modules
  test in build/order1/production/module fixtures with spaces/private linkage/.
  Script, C++ native harness, symbol dumps and logs remain in build/order5/.
  This is a small local inspection driver, not a production framework.
- Initial test build required adding the Runtime header for the new forced-GC
  fixture (backend.hpp only forward-declares Runtime). Fixed in the same sprint;
  final builds and focused tests pass. No production execution failure occurred.

Remaining limits/platform concerns: validation was local Linux x86-64. LLVM
internal linkage and existing target mangling are used without ELF-specific
production rules; Windows COFF and macOS Mach-O are not claimed tested. The
object-symbol regression tolerates the normal target prefix, but hosted CI is
still needed. Public/project and legacy native exports remain intentionally
conservative. Existing cross-generation raw native imports require the host to
retain provider leases; this order adds no dependency-lifetime registry. No
performance claim, new native FFI contract or source visibility change is made.

Orders 1-4 remain uncommitted and preserved. No commit, push, branch, remote
modification, full CI or unrelated feature work was performed. Order 5 is complete;
stop here until the owner authorizes consolidated validation/hosted CI.

## 2026-10-17 — Final consolidated validation, Orders 1–5

Completed the owner-authorized combined validation against the actual uncommitted
worktree. Read the five order entries, current continuity rules and relevant
source-array, module, source-compiler, sanitizer and editor documentation. All
five implementations remain present. No integration defect was demonstrated;
no compiler, Runtime, test, CMake or workflow repair was necessary. The tracked
worktree diff was byte-identical before and after testing, before this final
documentation update. Earlier owner changes were preserved.

Combined source reviewed:

- Order 1: default-OFF VOID_LLVM_RESEARCH gates storage_audit.cpp, encoded_cfg.cpp,
  inventory instrumentation, optional emitters and research tests. Ordinary
  lower/verify/optimize/JIT/object emission does not invoke certification.
- Order 2: the optional private-call fixture uses normal direct LLVM arguments,
  with Frame/Slot lifetimes and Runtime copy transactions. Envelope/CallBinding,
  duplicated authorization and custom machine-ABI reconstruction remain removed.
- Order 3: numeric/bool fixed arrays use Runtime heap buffers through the shared
  frontend/typed IR/LLVM JIT/AOT path. Independent snapshots, checked indexing,
  evaluation order, arguments/returns, precise roots and transactional publication
  are preserved. Large payloads are not allocated on the native stack.
- Order 4: matching-Frame reuse/early search termination, immutable source-layout
  reuse and conservative per-Heap alias bounds remain. Exact root lifetime,
  schema/packing/extent, lease and publication checks are still enforced.
- Order 5: unselected source-private definitions are LLVM-internal with matching
  debug local-unit flags. Actual selected entries, public/project exports,
  legacy native definitions, C/Runtime imports and external metadata remain
  resolvable. Source names, module ownership and ABI lowering are unchanged.

Local environment: Linux x86-64; installed Clang/LLVM/LLD 19.1.7 at /usr/lib/llvm-19,
C++20, RTTI ON matching the SDK. Existing build caches retain those explicit tool
paths. Debug and Release are unsanitized; sanitizer configuration is
RelWithDebInfo (-O2 -g -DNDEBUG) with -fsanitize=address,undefined and
-fno-omit-frame-pointer. No suppression/ignorelist or sanitizer disabling added.

| Final run | Outcome | Wall time |
| --- | --- | ---: |
| Complete production CTest, Debug, research OFF | 64/64 passed | 139.14 s |
| Complete production CTest, Release, research OFF | 64/64 passed | 55.24 s |
| Complete production CTest, strict ASan/UBSan/leaks, research OFF | 64/64 passed | 144.36 s |
| Optional research ON, auditor plus all private-call cases | 13/13 passed | 15.86 s |
| Debug frontend with 1 MiB stack | 1/1 passed | 9.03 s |
| Release frontend with 1 MiB stack | 1/1 passed | 4.68 s |

The 192 production suite executions cover ordinary/O2 JIT and linked native AOT,
all existing compiler/frontend/type/module/native tests, callbacks and hot reload,
precise GC/safepoints/allocation failures, diagnostics/tooling/sessions and LSP.
Each configuration executes all 116 fixed-array CLI cases, including expected
bounds failures and independent 2 MiB payloads. Strict sanitizer source-array
output records 215 failing allocation budgets in EACH normal/O2 mode, with
collection, cleanup and successful retry after each, plus ABI publication sweeps.
Order 4's root/layout/alias regressions pass in gc_buffers in all three builds.
The modules suite checks actual private/public object bindings, selected private
and imported JIT entries, private array calls/returns, old-generation leases and
separate LLVM-module import resolution. No correctness checks were weakened.

Eight additional large-array executions passed under a 1 MiB native stack:
normal/O2 JIT and linked AOT in both Debug and Release, each returning 53 for
input 7. The existing frontend small-stack regressions also passed as above.

Production inventories are identical across configurations: 64 tests, no
arrays_storage_audit or private_fixed_call research tests. All three default
backend archives contain only backend.cpp.o; llvm-nm finds neither research
emitter entrypoint. Ordinary successful array/aggregate/managed execution does
not depend on the optional research acceptance limits. No artificial array-size
limit was introduced.

Environment failures investigated and resolved without source changes:

- The tracing sandbox made LeakSanitizer abort explicitly with "does not work
  under ptrace", first in a small environment probe and then during the first
  sanitized CLI object generation. Make deleted the failed generated object.
  Continued the same build and ran the complete suite outside that sandbox,
  retaining detect_leaks=1, halt_on_error=1 and strict report exit settings.
- Chromium's sandbox operation was denied when launching real editor tests.
  Reran the unchanged test outside the host sandbox, using disposable profiles
  and the installed VS Code 1.141.0. No editor assertions were disabled.

Editor/package validation: Node 24.19.0, existing lockfile-installed dependencies.
`npm test` passed its configuration test and TypeScript/build steps. Real editor
trusted, restricted-trust and multi-root profiles all passed against the current
Debug voidls, including actual native-child lifecycle and failure cleanup.
`npm run package` successfully built the existing eight-file VSIX at
editors/vscode/dist/void-language-0.1.0.vsix (149139 bytes). SHA-256:
75686052f20787a1f351ee5a5c33e7e17765659a91f16ac9cf556be1c7926c6a.
No editor source/dependency changes were required.

Exact build/test commands (repository root, existing pinned build caches):

```sh
cmake -S . -B build/order1/production -DVOID_LLVM_RESEARCH=OFF
cmake --build build/order1/production -j 3
ctest --test-dir build/order1/production --output-on-failure -j 2
cmake -S . -B build/order4/release -DVOID_LLVM_RESEARCH=OFF
cmake --build build/order4/release -j 2
ctest --test-dir build/order4/release --output-on-failure -j 2
cmake -S . -B build/ci-sanitizers -DVOID_LLVM_RESEARCH=OFF -DVOID_SANITIZERS=ON
# Run outside the tracing sandbox; keep every sanitizer option enabled.
env ASAN_OPTIONS=detect_leaks=1:halt_on_error=1:exitcode=87 UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1:exitcode=86 cmake --build build/ci-sanitizers -j 2
env ASAN_OPTIONS=detect_leaks=1:halt_on_error=1:exitcode=87 UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1:exitcode=86 ctest --test-dir build/ci-sanitizers --output-on-failure -j 2
cmake -S . -B build/ci-debug -DVOID_LLVM_RESEARCH=ON
python3 - <<'PY'
import json, subprocess
x = json.loads(subprocess.check_output(['ctest', '--test-dir', 'build/ci-debug', '--show-only=json-v1'], text=True))
targets = ['void_arrays_storage_audit'] + ['void_' + t['name'] for t in x['tests'] if t['name'].startswith('private_fixed_call_')]
subprocess.run(['cmake', '--build', 'build/ci-debug', '--target', *targets, '-j', '2'], check=True)
PY
ctest --test-dir build/ci-debug -L research --output-on-failure -j 2
```

Small-stack commands, executed in separate shells for each build:

```sh
for build in build/order1/production build/order4/release; do
  (
    ulimit -s 1024
    ctest --test-dir "$build" -R '^frontend$' --output-on-failure
    "$build/voidc" run examples/fixed_arrays/large.void 7 --no-opt
    "$build/voidc" run examples/fixed_arrays/large.void 7
    "$build/void_fixed_arrays_cli_large_normal" 7
    "$build/void_fixed_arrays_cli_large_optimized" 7
  )
done
```

Editor commands, from editors/vscode:

```sh
export PATH=/home/shmellyorc/.nvm/versions/node/v24.19.0/bin:$PATH
npm test
npm run package
# Outside the tracing sandbox, with the existing desktop display:
VOIDLS_PATH=/home/shmellyorc/Documents/Void.Language/build/order1/production/voidls VSCODE_EXECUTABLE_PATH=/usr/share/code/code npm run test:host
```

Complete build/test logs, test inventories, sanitizer success output, original
sandbox diagnostics and backend symbol listings are in build/final-validation/.
The final combined uncommitted worktree patch there includes modified tracked
files AND the 24 new source/documentation files; it excludes ignored build/editor
products. It preserves earlier local owner changes as part of the combined state.

Readiness review: .github/workflows/ci.yml is unchanged and already configures
fresh default-OFF research builds, builds all production targets and runs
unfiltered CTest in Linux Debug/Release/Sanitizers and Windows/macOS Debug/Release.
It also retains strict sanitizer environment settings, Linux frontend small-stack
checks, Node 24.19.0, real editor tests and VSIX packaging. All intended includes,
research fixture sources, 11 fixed-array examples and new tests/docs are present.
The combined implementation is ready for the owner's commit/push and hosted CI.

Limitations: these results are local Linux evidence, not Windows/COFF or
macOS/Mach-O execution. The installed SDK enables RTTI/vptr coverage; the official
CI archive uses its existing no-RTTI ABI policy. LLVM's prebuilt libraries and
emitted JIT/AOT instructions are not rebuilt with ASan instrumentation; VOID's
C++ compiler/Runtime/tests are instrumented. Existing fixed-array scope limits,
eager copies/root retention and worst-case linear lookups remain documented.
No new feature or speculative optimization was added. No commits, pushes,
branches, remote modifications or hosted CI launches were performed. Stop here.

## 17 October 2026: Production numeric/bool growable arrays

Implemented the owner's complete growable-array feature against clean baseline
7170502f4b2cba4986264b51d4c046833148df40. The owner reports baseline CI #60 green
(7/7); no hosted run was launched or independently claimed for these changes.
Orders 1–5 remain in place: research is optional/OFF in the production build,
private calls use normal LLVM lowering, fixed arrays retain independent snapshots,
Runtime/root improvements remain, and function linkage policy is unchanged.

### Implementation

- The existing structural `[T]` type now supports all ten numeric types and bool
  in real source. Contextual empty literals, inferred nonempty literals, defaults,
  count (also fixed arrays), push/remove, signed/unsigned checked indexing and
  compounds use the existing frontend and typed IR. No later-statement inference
  for an unannotated empty literal was added.
- Growable handles participate in the existing logical reference leaf/capability
  model and precise root maps. Assignment, arguments, returns and aggregate copies
  preserve header identity; fresh literals/defaults are independent. An aggregate
  containing both fixed and growable arrays copies the fixed value independently
  while retaining the growable alias. Readonly authority survives aliasing and
  function/field propagation; source and independent IR negatives enforce it.
- Selected writes snapshot the rooted header, checked normalized index, structural
  version and (for compounds) old scalar before RHS evaluation. Rebinding does not
  redirect the write. RHS push/remove invalidates commit even when count is restored.
  Scalar element mutations during the RHS remain visible.
- `src/runtime/list.cpp` uses an ordinary existing GC object for the stable header
  and the existing scalar buffer/schema/BufferBuilder for replaceable storage.
  Empty headers have no buffer. Growth is geometric, arithmetic checked, copies
  poll under existing read leases, and publication updates the header only after
  a complete replacement and version check. In-capacity mutation/removal completes
  under the Runtime lock; unused tail bytes are cleared and live prefix updated.
  There is no new collector, allocator, ownership registry or certification scheme.
- Shared LLVM lowering binds ordinary Runtime C helpers for both ORC and AOT;
  LLVM handles the native ABI. Growable values are rooted pointer-sized handles,
  not fixed-array snapshots or native payload allocas. The existing CMake CLI
  fixture loop is shared by fixed/growable families rather than copied.
- Added seven ordinary source examples, the focused `growable_arrays_source`
  harness and `growable_arrays_cli` integration suite. Replaced the now-obsolete
  foundation assertion rejecting `[i32]` with a still-unsupported nested-list
  negative; positive production list tests cover the newly supported type.
  README, FIXED_ARRAYS, ARRAYS_PROPOSAL and WRENCH link the new GROWABLE_ARRAYS doc.

### Genuine failures found and repaired

The initial list-parameter lowering reached the old numeric debug-type fallback
and threw "Not a numeric type". Array pointer debug types now cover both kinds.
The focused reference-value regression then exposed an overly broad readonly view
on pure module templates: an existing null reference field became permanently
readonly, preventing replacement with a fresh mutable object. Constant materialization
now adds readonly authority only to actual list leaves, preserving existing null
reference-template behavior. The original `reference_values/templates.void` passes,
and a new negative prevents thawing a list field inside a constant value template.
No existing safety assertion or sanitizer was disabled.

### Focused local verification

LLVM: `/usr/bin/llvm-config-19 --version` reports **19.1.7**. Default production
configuration has `VOID_LLVM_RESEARCH:BOOL=OFF`. Final Debug focused CTest run:
**18/18 passed**, 34.38 seconds. This includes the new source/CLI suites, fixed
source/CLI arrays, array foundation, GC buffers/schemas, coordination/generated
Runtime, managed/reference values, core, frontend, analysis, tooling, modules,
JIT and reload. The growable CLI suite executes **80** normal/O2 JIT/linked-AOT
cases, including 16 expected checked failures. A subsequent mixed fixed/list
field-copy regression rerun passed both growable suites.

The source harness additionally passed these deterministic allocation sweeps:
15 failing budgets for growth, 1 for in-capacity push, 1 for removal, 12 for a
replacement empty header, 48 for cold metadata/header/backing construction, and
35 each for normal/O2 source invocation. Every failed budget verifies unchanged
published state, collection/cleanup and successful retry. Wrong scalar encoding,
unregistered root words, unrepresentable extent and stale selected versions
reject. Concurrent collection parks live list operations and preserves all 4097
elements through resizing; all roots balance and all objects collect afterwards.
All numeric/bool element types and all eight integer index types execute; signed
minimum and unsigned maximum indexes fail safely. Readonly and malformed IR
negatives pass.

Strict final ASan/UBSan/leak result: **5/5 passed**, 18.54 seconds (growable
source, fixed source, reference-value source, GC buffers and coordination).

The large source example grows 262145 u64 elements (more than 2 MiB live payload,
4 MiB backing capacity), aliases/mutates/removes and returns **262151** for input
7 through all four modes under a **1 MiB stack**. All four runs passed.

Reproducible build commands and exact focused test commands (from repository root;
existing pinned build directories):

```sh
cmake -S . -B build/order1/production
python3 - <<'PY'
import subprocess
families = [('growable_arrays', 'main aliases selection invalidation indexes numeric large'),
            ('fixed_arrays', 'main copies order numeric indexes index_types empty large value_fields bounds_order loops')]
targets = ['void_' + family + '_cli_' + program + '_' + mode
           for family, programs in families for program in programs.split()
           for mode in ['normal', 'optimized']]
targets += ['void_fixed_arrays_tests', 'void_growable_arrays_tests', 'void_gc_buffers',
            'void_gc_schemas', 'void_runtime_tests', 'void_runtime_generated',
            'void_managed_tests', 'void_reference_values_tests', 'void_tests']
subprocess.run(['cmake', '--build', 'build/order1/production', '--target', *targets, '-j2'], check=True)
PY
ctest --test-dir build/order1/production -R '^(growable_arrays_source|growable_arrays_cli|fixed_arrays_source|fixed_arrays_cli|arrays_foundation|gc_buffers|gc_schemas|runtime_coordination|runtime_generated|managed_source|reference_values_source|core|frontend|analysis|tooling|modules|jit|reload)$' --output-on-failure -j2
build/order1/production/void_growable_arrays_tests .
cmake --build build/order1/production --target void_growable_arrays_cli_aliases_normal void_growable_arrays_cli_aliases_optimized -j2
ctest --test-dir build/order1/production -R '^growable_arrays_(source|cli)$' --output-on-failure
cmake -S . -B build/ci-sanitizers -DVOID_LLVM_RESEARCH=OFF -DVOID_SANITIZERS=ON
# Outside the tracing sandbox, retaining all strict sanitizer settings:
env ASAN_OPTIONS=detect_leaks=1:halt_on_error=1:exitcode=87 UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1:exitcode=86 cmake --build build/ci-sanitizers --target void_growable_arrays_tests void_fixed_arrays_tests void_reference_values_tests void_gc_buffers void_runtime_tests -j2
env ASAN_OPTIONS=detect_leaks=1:halt_on_error=1:exitcode=87 UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1:exitcode=86 ctest --test-dir build/ci-sanitizers -R '^(growable_arrays_source|fixed_arrays_source|reference_values_source|gc_buffers|runtime_coordination)$' --output-on-failure
(
  ulimit -s 1024
  build/order1/production/voidc run examples/growable_arrays/large.void 7 --no-opt
  build/order1/production/voidc run examples/growable_arrays/large.void 7
  build/order1/production/void_growable_arrays_cli_large_normal 7
  build/order1/production/void_growable_arrays_cli_large_optimized 7
)
git diff --check
```

Detailed logs are in build/growable/: final-tests.log, mixed-fields-tests.log,
source-details.log, final-sanitizer-tests.log and small-stack.log. Existing
GitHub Actions builds all production targets and runs unfiltered CTest, so the
new suites are included without a separate workflow or test framework.

Limitations: numeric/bool elements only; nested/reference/string/struct elements,
list equality, iteration/ranges and other collection APIs remain deferred.
Removal is linear, retains capacity and holds the Runtime lock during memmove;
compiler temporary roots retain existing lifetimes. Physical size/count/version
representability is checked; no arbitrary 64 KiB ceiling is imposed. This sprint
provides local Linux evidence, not Windows/macOS execution or a full CI matrix.
Sanitizers instrument VOID C++ compiler/Runtime/tests; the prebuilt LLVM SDK and
generated JIT/AOT machine instructions are not rebuilt with instrumentation.
No commits, pushes, branches, remote changes or hosted CI launches were performed.

## Phase 23 continuation — complete source arrays, 17 October 2026

Implemented strings, value structs, managed/nullable and interface references,
and nested fixed/growable arrays in the existing production frontend, typed IR,
shared LLVM 19.1.7 lowerer and coordinated Runtime. Fixed children copy recursively;
growable and managed fields retain identity. Recursive value types through list
fields work, including cyclic managed graphs. Required references need explicit
values; zero-length arrays remain valid. Per-element defaults, including custom
initializers, execute independently. Pure defaults retain compact repeated IR;
the shared lowerer outlines their validated pure recipes as private ordinary
functions instead of expanding IR by element count. There is no new backend,
collector, ownership registry, verifier protocol or arbitrary storage ceiling.

Composite elements reuse the existing schema catalog, DataCursor, BufferBuilder,
read leases and Frame/Slot roots. One generic list mutation implementation serves
both scalar and composite APIs. Native element records describe LLVM DataLayout
fields; the Runtime owns admitted metadata after code retirement. Fixed construction
uses an unpublished initialized prefix, and nested copying completes before result
publication. Failure paths and safepoints retain precise roots. Hot reload preserves
compatible array-held interface witnesses without retaining allocating code.

Nested selections keep rooted owners, normalized indexes and dependent structural
versions. Inline writes reacquire current parent values and preserve unrelated RHS
writes; independently captured nested list/managed identities end the containing
storage dependency. Logical homogeneous element profiles preserve readonly authority
without per-index metadata. Mutable list element profiles are invariant; copied
value storage cannot restore readonly reference authority.

Source `for element[, original_index] in [reverse] array:` traverses without a
collection/reversal copy. Item values follow ordinary copy/reference semantics;
index binders are original i64 indexes. Nested loops, continue, break and return
retain existing behavior. Replacing an element is nonstructural. The owner resolved
A6 explicitly: check a list's captured version at the next access **including normal
completion**; mutation followed immediately by break/return is allowed. No active
iterator registry or reverse-removal exception was added.

`a..b` and `a..<b` accept matching i32/i64 endpoints, evaluated once left to right,
ascending or descending, optionally reversed. Equal inclusive/exclusive ranges are
singleton/empty. Terminal checks precede stepping; full integer boundary ranges do
not compute a span or materialize storage. `0..<items.count` uses i64 as approved.

The existing TextMate grammar assigns `and`, `or`, `not` to the same keyword scope
as `as`, `if`, `return`, preserving `as`; traversal keywords are included. Extension
unit checks, packaging and real editor validation use the existing infrastructure.

Defects found and repaired during validation:

- Source composite-default lowering initially expanded pure IR and violated the
  existing compact-template assertions. Restored the compact representation and
  used ordinary private default-recipe functions; original assertions remain.
- The test construction probe canceled an enclosing allocation-failure sweep after
  the first managed object. It now restores only its own scoped override. The
  source suite now exercises **265 failing budgets per JIT mode**, plus **275 per
  mode for per-element custom defaults**, with collection,
  unchanged publication and successful retry after each failure.
- The unchanged 1 MiB semantic-depth regression exposed excessive native stack
  frames in recursive receiver hints/method checking. Debugger reproduction located
  these frames. Isolated qualified-node temporaries and post-receiver member checks
  from recursive dispatch, retaining the existing 256-depth guard, diagnostics and
  resource-limit assertions. No stack limit or assertion was relaxed.
- Updated obsolete "iteration unsupported"/numeric-only test expectations to actual
  invalid iterable/type cases, and removed stale numeric-only diagnostics.

Focused evidence: 13/13 directly affected Runtime, array, reference, interface,
inheritance and initialization suites passed before consolidation. Native reference
transaction sweeps cover 19 failing growth budgets, 4 in-capacity append, 3 removal,
and 62 cold metadata/header/backing construction budgets. Concurrent collection
preserves 257 reference elements through growth. Additional source cases cover
nullable interface/managed elements, recursive fixed defaults, composite function
arguments/results, readonly rejection, structural failure/recovery, independent
nested copies and code retirement/reload. `arrays_cli` executes **152** source
normal/O2 JIT/linked-AOT cases, including **24 expected checked failures**.

Reproducible final commands (repository root unless noted; pinned existing build
directories, production research OFF):

```sh
cmake --build build/order1/production -j2
ctest --test-dir build/order1/production --output-on-failure -j2
cmake --build build/order1/production --target void_tests voidc -j2
(ulimit -s 1024; ctest --test-dir build/order1/production -R '^frontend$' --output-on-failure)
python3 tests/integration/semantic_depth.py build/order1/production/voidc
# All three large programs, normal/O2 JIT and linked AOT, under a 1 MiB stack:
(ulimit -s 1024; python3 - <<'PY'
import subprocess
for family, expected in [('fixed_arrays', '53'), ('growable_arrays', '262151'), ('arrays', '14')]:
    for mode, flags in [('normal', ['--no-opt']), ('optimized', [])]:
        commands = [['build/order1/production/voidc', 'run', 'examples/' + family + '/large.void', '7', *flags],
                    ['build/order1/production/void_' + family + '_cli_large_' + mode, '7']]
        for command in commands:
            result = subprocess.run(command, capture_output=True, text=True, check=True)
            assert result.stdout.strip() == expected, (command, result.stdout)
PY
)
# Outside the tracing sandbox, retaining strict checks throughout build/test:
env ASAN_OPTIONS=detect_leaks=1:halt_on_error=1:exitcode=87 UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1:exitcode=86 cmake --build build/ci-sanitizers -j2
env ASAN_OPTIONS=detect_leaks=1:halt_on_error=1:exitcode=87 UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1:exitcode=86 ctest --test-dir build/ci-sanitizers --output-on-failure -j2
# editors/vscode, with the existing pinned Node 24.19.0 in PATH:
npm test
VOIDLS_PATH=/home/shmellyorc/Documents/Void.Language/build/order1/production/voidls VSCODE_EXECUTABLE_PATH=/usr/share/code/code npm run test:host
npm run package
# Repository root, actual installed VS Code TextMate/Oniguruma engine:
env ELECTRON_RUN_AS_NODE=1 /usr/share/code/code build/phase23/keyword-scopes.cjs
git diff --check
```

Final production Debug CTest: **68/68 passed**, 170.14 seconds. The strengthened
custom-default failure harness also passed after consolidation (275 failing budgets
in each JIT mode), using `cmake --build build/order1/production --target
void_arrays_source -j2` and `build/order1/production/void_arrays_source .`.
The unchanged frontend suite passed under a 1 MiB stack (11.28 seconds); its
standalone deep-receiver check passed. All **12** large-array JIT/AOT runs passed
under that same stack limit (fixed 2 MiB, growable >2 MiB, composite 2 MiB).

Editor unit checks passed; all three actual editor-host modes passed (trusted,
restricted trust, multi-root rejection). The installed VS Code TextMate/Oniguruma
engine confirmed identical keyword.control.void scopes for if/or/and/not/as/return
and preserved comment/string scopes. VSIX packaging succeeded (8 files, 145.64 KiB),
at editors/vscode/dist/void-language-0.1.0.vsix.

Strict ASan/UBSan with leak detection: **68/68 passed**, 208.37 seconds,
including the strengthened composite/default failure sweeps, source CLI depth
regression, normal/O2 JIT and linked AOT. No sanitizer or leak failures remain.
Both builds use LLVM 19.1.7 and VOID_LLVM_RESEARCH=OFF.

Final logs: build/phase23/final-build.log, final-tests.log,
sanitizer-final-build.log, sanitizer-final-tests.log,
custom-default-failures-tests.log, stack-repair2-test.log, semantic-depth.log,
large-small-stack.log, editor-final-unit.log, editor-final-host.log,
editor-final-package.log and editor-scopes.log. The local tokenizer script is
build/phase23/keyword-scopes.cjs; no new verification framework or dependency was
introduced. Existing GitHub Actions builds all production targets and runs
unfiltered CTest, so arrays_source/arrays_cli are included automatically.
The combined source is ready for the owner's hosted cross-platform acceptance gate.

Remaining scope/costs: slices, collection equality/search/sorting, generics, custom
iteration and string indexing remain future language work. Inline mutating methods
on array elements still reject as required by A4; explicit nested assignments work.
Eager fixed snapshots and existing temporary-root lifetimes can retain/copy extra
storage. List removal remains linear under the Runtime lock. Physical extent,
address and version representability remain checked. No cross-platform execution
or hosted CI is claimed. The prebuilt LLVM SDK and generated machine code are not
rebuilt with C++ sanitizer instrumentation. Orders 1–5 and previous source-array
work are preserved; research certification remains optional. No commits, pushes,
branches, remote changes or hosted CI launches were performed.

## Phase 23 — macOS allocation-sweep correction, 17 October 2026

CI run [39085394432](https://github.com/Shmellyorc/Void.Language/actions/runs/39085394432)
fails `growable_arrays_source` in macOS ARM64 Debug/Release. Both job logs show
operation 0 completing 13 failing allocation budgets, then `Runtime list allocation
sweep incomplete` before operation 1 is printed. In this test, that means the
in-capacity append succeeded at budget zero: `success` is true, `failures` is zero,
and the unconditional `failures>0` assertion rejects a legitimate success.

Inspected `Runtime::list_push/list_remove`, `fixed_scalar_schema`, and the shared
`array_push/array_remove` implementations. Warm scalar in-capacity append copies
into existing backing and updates count/version; removal shifts/clears existing
storage and updates count/version. Neither requires new backing or a new header.
Incidental standard-library allocations, including lookup-key strings, are not a
portable requirement. Locally both paths happen to exercise one failing budget;
requiring that allocation is the test defect, not evidence of a Runtime failure.

Minimal correction: require at least one allocation failure only for growth
(operation 0) and new-header construction (operation 3). All operations still
sweep budgets until success, and any failures still run the original rollback,
count/version, element-integrity, collection and retained-object assertions.
Root lifetime, stale-version, wrong-encoding, unregistered-root, cold construction,
concurrent collection and source normal/O2 failure/recovery checks are unchanged.
No production Runtime or other tests changed.

Focused Linux LLVM 19.1.7 results: Debug **1/1 passed**, 10.31 seconds; strict
ASan/UBSan/leak **1/1 passed**, 7.11 seconds. Both report failing budgets
**14/1/1/12** for operations 0–3, **48** for cold construction, and **42** each for
normal/O2 source invocation. The Debug baseline had the same counts. Exact commands:

```sh
cmake --build build/order1/production --target void_growable_arrays_tests -j2
ctest --test-dir build/order1/production -R '^growable_arrays_source$' --output-on-failure
# Outside the tracing sandbox, preserving strict sanitizer settings:
env ASAN_OPTIONS=detect_leaks=1:halt_on_error=1:exitcode=87 UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1:exitcode=86 cmake --build build/ci-sanitizers --target void_growable_arrays_tests -j2
env ASAN_OPTIONS=detect_leaks=1:halt_on_error=1:exitcode=87 UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1:exitcode=86 ctest --test-dir build/ci-sanitizers -R '^growable_arrays_source$' --output-on-failure
git diff --check
```

Logs: build/phase23-macos-sweep/ and the respective CTest LastTest.log files.
macOS execution remains for the owner's CI verification; it was not reproduced
on this Linux host. No commits, pushes, branches, remote changes or CI reruns.

## Phase 24 — native generic functions and structs (17 October 2026)

Baseline: local HEAD was verified as `ca22f1b5800b291e4741f048b28a19627b324839`,
Phase 23 accepted at owner-provided CI #62. The initial worktree was clean.
The owner's Work-first preference supersedes the earlier environment-routing
advice; long sprints, KISS/DRY, focused development tests and owner-controlled
commits/hosted acceptance remain unchanged.

Owner decisions applied: validate syntax/declaration structure immediately,
type-check bodies per concrete specialization, enforce interface constraints
before accepting instances while permitting additional concrete operations;
use `func (b box<t>) get() -> t:` to bind owner parameters; allow constraints on
value/managed structs and existing interface references via shared exact/access
conformance without boxing. No operator/comparable or variance contract invented.

Implementation:

- The existing parser retains template tokens and reparses a requested concrete
  instance with type bindings. Original Units own ordinary concrete declarations;
  identical nominal argument lists reuse instances. Existing layout, construction,
  defaults, semantic checking/effects, typed IR and LLVM 19.1.7 JIT/AOT are reused.
- Explicit/inferred generic functions, managed/value structs, nested arguments,
  generic-to-generic calls, recursive calls/types, receiver owner parameters and
  separate method parameters execute in real source programs. Constructor
  inference uses supplied fields/init parameters. Conflicting/ambiguous inference
  diagnoses; existing exact conversions and source evaluation order remain.
- Interface constraints use the existing structural selector/signature checker;
  managed interface erasure retains its original representation boundary.
  Concrete root layouts, independent fixed snapshots, shared lists/references,
  readonly authority, safepoints, publication and cleanup use existing facilities.
  No Runtime, collector, allocator or LLVM calling-convention changes.
- Template body errors retain body and requesting call/declaration spans. Native
  identity stays canonical; debug/trace names and source navigation use original
  declarations. Original type parameters/references publish once. Existing module
  access, private linkage, selected entry exports, reload contracts and generation
  leases remain in force.
- Dependency depth 128 and a per-analysis specialization-identity work budget
  8,388,608 characters bound pathological expansion. Ordinary recursion reuses
  its instance. There is no parameter-count or array-payload ceiling.
- Added examples/generics, tests/generics and the existing CLI/AOT CMake family;
  GENERICS.md documents actual syntax, approved decisions and exclusions. Bible,
  interface guide and WRENCH status now distinguish working constraints from
  future boxing/variance/operator syntax.

Defects caught and corrected during integration:

- The initial body worklist skipped any function carrying a recovered parse/lex
  error. Existing string recovery, UTF-8 snapshot and retained-analysis recovery tests demonstrated that this
  suppressed independent body diagnostics. Restored ordinary recovery analysis;
  the analyzed flag alone prevents worklist repeats. Existing assertions remain
  unchanged. Focused strings_source/generics_source/frontend then passed 3/3.
- Unused template header checks now reuse existing declaration-shadow checks and
  parameter-default structure rules. Fixed-array extent validation was factored
  from existing array resolution so malformed unused annotations also diagnose;
  dependent layouts/bodies still wait for concrete types.
- In the new test fixture only, root-probe frame storage is reserved before an
  allocation sweep: its noexcept observation hook must not allocate while the
  injected failure budget is active. Root/frame assertions remain enabled.
  Fixture identifiers that violated existing no-shadow rules were corrected;
  production shadowing rules were preserved.
- An unused-template navigation regression exposed missing references to named
  constraint/annotation types. Header navigation now uses existing source-name
  lookup without instantiating the template or checking an unused body. Added
  regression checks for unused constraints and nominal annotations. The focused
  generics suite passed after correction. Deferred body semantic references are
  documented rather than fabricated.

- A reproduced method-specific generic declaration could hide an inherited
  ordinary method because it was absent from ordinary concrete headers until
  called. Declaration and specialization checks now reuse the shared member
  selector to preserve the existing no-hiding rule. Ordinary and generic receiver
  regressions cover both paths; generics_source/inheritance_source passed 2/2.
- The new reload fixture now retains the existing SourceFile UTF-8 path instead
  of relying on a native filesystem-path-to-string conversion (not portable to
  Windows). Its focused Debug/Release/strict-sanitizer checks passed. No Windows
  execution is claimed.

Validation results and exact commands follow below.

Reproducible local commands (existing cached configurations, all research OFF;
LLVM reports **19.1.7**):

```sh
/usr/lib/llvm-19/bin/llvm-config --version
cmake -S . -B build/order1/production
cmake -S . -B build/ci-release -DVOID_LLVM_RESEARCH=OFF
cmake -S . -B build/ci-sanitizers -DVOID_LLVM_RESEARCH=OFF

# Focused development/repair checks:
ctest --test-dir build/order1/production -R '^(frontend|tooling|modules|arrays_source|interfaces_source|reload)$' --output-on-failure -j2
ctest --test-dir build/order1/production -R '^(strings_source|generics_source|frontend)$' --output-on-failure -j2
ctest --test-dir build/order1/production -R '^generics_source$' --output-on-failure

# Complete corrected source, existing Debug and Release configurations:
cmake --build build/order1/production -j3
ctest --test-dir build/order1/production --output-on-failure -j2
cmake --build build/ci-release -j3
ctest --test-dir build/ci-release --output-on-failure -j2

# Strict sanitizer configuration: executed outside the tracing sandbox for LSan.
ASAN_OPTIONS=detect_leaks=1:halt_on_error=1:exitcode=87 UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1:exitcode=86 cmake --build build/ci-sanitizers -j3
ASAN_OPTIONS=detect_leaks=1:halt_on_error=1:exitcode=87 UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1:exitcode=86 ctest --test-dir build/ci-sanitizers --output-on-failure -j2

(ulimit -s 1024; ctest --test-dir build/order1/production -R '^frontend$' --output-on-failure)
python3 tests/integration/semantic_depth.py build/order1/production/voidc
(ulimit -s 1024; build/order1/production/voidc run examples/generics/large.void 7 --no-opt && build/order1/production/voidc run examples/generics/large.void 7 && build/order1/production/void_generics_cli_large_normal 7 && build/order1/production/void_generics_cli_large_optimized 7)

PATH=/home/shmellyorc/.nvm/versions/node/v24.19.0/bin:$PATH npm --prefix editors/vscode test
PATH=/home/shmellyorc/.nvm/versions/node/v24.19.0/bin:$PATH VOIDLS_PATH=/home/shmellyorc/Documents/Void.Language/build/order1/production/voidls VSCODE_EXECUTABLE_PATH=/usr/share/code/code npm --prefix editors/vscode run test:host
PATH=/home/shmellyorc/.nvm/versions/node/v24.19.0/bin:$PATH npm --prefix editors/vscode run package
git diff --check
```

The initial six adjacent suites passed 6/6; diagnostic-recovery repair checks
passed 3/3, and the final focused generics test passed. The real editor passed
trusted-project diagnostics/unsaved repair/navigation/lifecycle checks, restricted
trust (no native child), and multi-root rejection (no native child). Extension
Node tests/typechecking/build and VSIX packaging passed. `xvfb-run` is absent on
this host; the real editor ran successfully on its available local display.

Final corrected production validation (logs in ignored `build/phase24/`):

| Configuration | Complete CTest result | Elapsed |
| --- | --- | --- |
| Debug (`order1/production`) | **70/70 passed** | 182.66 s |
| Release (`ci-release`) | **70/70 passed** | 86.61 s |
| Strict ASan/UBSan + leak detection (`ci-sanitizers`) | **70/70 passed**, no sanitizer/leak findings | 198.90 s |

Final logs: `validation-debug.log`, `validation-release.log`,
`validation-sanitizer.log`; matching `*-build.log` files contain build evidence.
No assertions were weakened, sanitizer settings disabled or production tests
removed. Superseded runs exposed the recovery defect above and were repaired;
sanitizer runs on superseded source were stopped before final validation.

The new `generics_cli` suite checks **96 successful executions** (12 source
programs, inputs 0/7, normal/O2, JIT/independently linked AOT), plus four checked
bounds failures retaining the original generic-body source/function trace.
`generics_source` passes all three configurations, including independent fixed
copies, list aliases, nested/recursive generic types and cycles, exact numeric
boundaries, strings/interfaces, owner/method parameters, constraints, argument
ordering, original-source navigation, selected entries, compatible reload/old
leases and metadata/object retirement after executable generation release.
Root inspection and transactional construction publication remain asserted.

Each configuration sweeps **354** failing allocation budgets for the reference/
array fixture and **231** for generic initialization, in both normal and O2:
**1,170 intentional allocation failures per configuration**, followed by successful
budgets. Every failure verifies balanced valid roots, unchanged publication,
collection to an empty heap, and a successful retry. Construction/growth are real
allocating paths; no allocation-free Runtime operation is forced to fail.

Final Debug and Release frontend tests both passed with a **1 MiB stack**;
`semantic_depth.py` passed for both compilers, preserving recovery/nesting spans.
The 2 MiB generic payload returned 14 through normal/O2 JIT and linked AOT under
that stack in both configurations (eight executions). Release commands were:

```sh
(ulimit -s 1024; ctest --test-dir build/ci-release -R '^frontend$' --output-on-failure)
python3 tests/integration/semantic_depth.py build/ci-release/voidc
(ulimit -s 1024; build/ci-release/voidc run examples/generics/large.void 7 --no-opt && build/ci-release/voidc run examples/generics/large.void 7 && build/ci-release/void_generics_cli_large_normal 7 && build/ci-release/void_generics_cli_large_optimized 7)
```

The final rebuilt Debug voidls also passed the real editor's trusted, restricted
and multi-root runs (`validation-editor-host.log`). The local package is
`editors/vscode/dist/void-language-0.1.0.vsix`. `git diff --check` passed.

Remaining boundaries are documented in GENERICS.md: general `t?`, generic
interfaces, method-specific generic virtuals/initializers, variance/operators and
more aggressive code sharing are outside this foundation. Method-specific
generic selection currently requires a direct receiver declaration; unused bodies
have header/type navigation and gain semantic body references when specialized.
Existing owner-parameter methods use ordinary concrete inheritance/composition.
There are no known failing local production tests. Windows/macOS acceptance
remains the owner's hosted gate; this sprint makes no cross-platform pass claim.

Phase 23 and Orders 1–5 remain intact. LLVM stays 19.1.7; Runtime/GC and optional
research acceptance boundaries are unchanged. HEAD remains the accepted baseline;
all delivery changes are uncommitted. No branches, commits, pushes, remote
modifications or hosted CI launches were performed. Ready for owner review and
hosted cross-platform validation.


## 2026-10-18 — Phase 25: expanded array operations

Baseline: owner-locked Phase 24 `47cb73c300bc9ea1cef6fcc6789eb6b481e438d0`,
CI #63 (seven green jobs). Implemented the owner's approved insertion, clear and
slice contract; Phase 24 and Orders 1–5 remain intact. LLVM remains 19.1.7 and
research remains OFF in all production validation configurations.

- Growable `.insert(index, value)` accepts positions 0 through count, including
  existing negative element normalization. Receiver/index selection and boundary
  checking precede value evaluation; captured version rejects structural RHS
  mutation while preserving earlier side effects. Push and insert share the
  existing locked in-capacity path and transactional leased-buffer growth path.
- `.clear()` preserves list identity/capacity, zeros removed elements and updates
  the traced live prefix under the Runtime lock. Nonempty clear increments the
  version once; empty clear is a no-op. Removed references become collectible.
- Fixed/growable `source[first..last]` and `source[first..<last]` create fresh
  growable collections, support negative/mixed integer endpoints and normalized
  forward/reverse direction, and reject invalid bounds without clamping. Equal
  exclusive boundary endpoints are empty. Receiver/start/end evaluate once in
  order; fixed receivers snapshot before endpoint effects, list receivers retain
  identity and copy current contents afterwards. Fixed children copy recursively;
  lists/managed references preserve identity and readonly authority.
- Reused the existing semantic checker, IR capability propagation/validation,
  precise Frame/Slot roots, BufferBuilder/read leases, and shared LLVM JIT/AOT
  lowerer. Extracted the existing fixed-child copy loop for use by slicing. No
  additional collector, collection implementation, registry or verification system.
- Added eight real source fixtures and `collections_source` / `collections_cli`
  to the existing production CTest workflow. They exercise all element families,
  generic instances, readonly errors, evaluation order, aliases, fixed snapshots,
  iterator invalidation, reference reclamation, failure rollback/retry, roots,
  publication, hot reload/old leases and generic source navigation.
- Owner addition: `.github/workflows/ci.yml` job timeout changed from 90 to
  **120 minutes** (two hours); the existing seven-job matrix is unchanged.

A demonstrated frontend integration defect was fixed: inference revisited an
already checked constructor inside an unannotated nested array literal, dispatched
it as a binary expression and accessed nonexistent operands. Reuse its checked
construction type on that revisit; the normal contextual compatibility check still
runs. Both readonly rejection and positive nested-construction/slice cases cover
this path. The initial CLI invalidation regression also exposed an opaque generic
array failure message; insertion now reports selected insertion invalidation/failure
with its original source location. Tests retain strict checked-failure assertions.

Focused validation before consolidated builds:

```sh
cmake --build build/order1/production --target void_collections_tests void_arrays_source void_generics_tests void_tests -j3
ctest --test-dir build/order1/production -R '^(collections_source|arrays_source|generics_source|frontend)$' --output-on-failure -j2
ctest --test-dir build/order1/production -R '^(collections_source|collections_cli)$' --output-on-failure -j2
```

Results: adjacent suite **4/4 passed** (61.46 s); final feature JIT/AOT suites
**2/2 passed** (63.94 s). Feature CLI covers **120 executions**: 76 successful
and 44 checked failures through normal/O2 JIT and independently linked AOT.
Source sweeps cover 276 failing budgets for main and 2,170 for nested in each
codegen mode, with valid balanced roots, transactional publication, collection
to an empty heap and successful retry after every failure. Native sweeps separately
cover growth (19), in-capacity insertion (4) and slicing (36) failing budgets on
this Linux build; allocation-free paths are allowed to succeed at budget zero.
Logs and exact build evidence are under ignored `build/phase25/`.

Consolidated validation commands (existing configured Debug, Release and strict
RelWithDebInfo sanitizer builds; all with `VOID_LLVM_RESEARCH=OFF`):

```sh
cmake --build build/order1/production -j3
ctest --test-dir build/order1/production --output-on-failure -j2
cmake -S . -B build/ci-release -DVOID_LLVM_RESEARCH=OFF
cmake --build build/ci-release -j3
ctest --test-dir build/ci-release --output-on-failure -j2
export ASAN_OPTIONS=detect_leaks=1:halt_on_error=1:exitcode=87
export UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1:exitcode=86
cmake -S . -B build/ci-sanitizers -DVOID_LLVM_RESEARCH=OFF
cmake --build build/ci-sanitizers -j3
ctest --test-dir build/ci-sanitizers --output-on-failure -j2
```

Sanitizer execution uses the ordinary host environment outside the tracing sandbox
so LeakSanitizer can run. No sanitizer is disabled and no findings are suppressed.

Debug and Release also pass the existing frontend and deep-receiver regressions
with a **1 MiB native stack**, plus all four normal/O2 JIT/linked AOT executions of
the 2 MiB collection fixture (each returns 14). Exact commands, run for each build:

```sh
for configuration in order1/production ci-release; do
  (ulimit -s 1024; ctest --test-dir build/$configuration -R '^frontend$' --output-on-failure)
  python3 tests/integration/semantic_depth.py build/$configuration/voidc
  (ulimit -s 1024; build/$configuration/voidc run examples/collections/large.void 7 --no-opt && build/$configuration/voidc run examples/collections/large.void 7 && build/$configuration/void_collections_cli_large_normal 7 && build/$configuration/void_collections_cli_large_optimized 7)
done
```

The final rebuilt Debug voidls passes the existing real VS Code trusted, restricted
trust and multi-root integration runs. Extension typecheck/build/test and VSIX
packaging also pass. Commands (Node 24.19.0 on PATH):

```sh
npm --prefix editors/vscode test
VOIDLS_PATH=/home/shmellyorc/Documents/Void.Language/build/order1/production/voidls VSCODE_EXECUTABLE_PATH=/usr/share/code/code npm --prefix editors/vscode run test:host
npm --prefix editors/vscode run package
```

The local deliverable is `editors/vscode/dist/void-language-0.1.0.vsix`. Evidence:
`build/phase25/editor-tests.log`, `editor-host.log`, `vsix.log`,
`debug-stack.log` and `release-stack.log`. Existing CTest coverage includes LSP,
hot reload, cross-module/entry/linkage, native packaging and Runtime regressions.

Final consolidated results on the corrected source:

| Configuration | Production CTest | Elapsed |
| --- | --- | --- |
| Linux Debug | **72/72 passed** | 254.83 s |
| Linux Release | **72/72 passed** | 108.30 s |
| Strict ASan/UBSan + leak detection | **72/72 passed**, no sanitizer/leak findings | 239.64 s |

Final build/test logs are `build/phase25/{debug,release,sanitizer}-build.log` and
`{debug,release,sanitizer}-tests.log`; corresponding `*-test-details.log` files
preserve successful-test output and failure-sweep evidence. All three configurations
report the same collection allocation-sweep counts above. No test assertions were
weakened, production tests removed or sanitizer settings relaxed.

Remaining boundaries: `.pop()` deliberately awaits result/Ok; omitted slice
endpoints, slice assignment/views, string slicing and unrelated collection features
are outside this approved delivery. Compiler temporaries retain ordinary function
root lifetimes. There are no known failing local production tests. Windows/macOS
verification remains the owner's hosted acceptance gate; no cross-platform pass
claim is made. Updated ARRAY_OPERATIONS.md and the relevant language/array/generic
contracts document the implemented behavior and supersede historical exclusions.

`git diff --check` passed. HEAD remains the accepted baseline and all changes are
uncommitted. No branches, commits, pushes, remote modifications or hosted CI
launches were performed. Ready for owner review and hosted cross-platform CI.


## 2026-10-18 — Original Stage 3: managed Runtime completion

Accepted baseline: local HEAD verified at `c946510a0a72428d7d5849007f8328753377cdf6`,
initial worktree clean. The owner confirms CI #64, all seven jobs green, locking
Phases 1–25. No hosted run was launched/fetched for this sprint; the next hosted
acceptance remains owner-controlled. Original roadmap Stage 3 is distinct from the
historical Phase 23 research stages. LLVM remains 19.1.7.

The owner approved one grouped dictionary decision: colon expression-key literals,
ordinary identifier expressions, integer/bool/string/managed-identity and recursively
eligible value keys, shared dictionary identity, checked indexing/upsert, bool
contains/remove, clear/count, captured receiver/key/version before RHS, ordered
forward/reverse value/key traversal and next-access/completion invalidation.
Result-based get and float/interface/collection/custom-equality keys are deferred.

### Delivered implementation and integration

- Added structural `{K: V}` types, contextual/inferred multiline literals, safe
  empty defaults and pure module/field/parameter templates to the existing parser,
  checker, generic specialization/inference and typed IR. Literal key/value
  evaluation is ordered and duplicate keys preserve the original insertion place.
- Added one managed dictionary header with an ordered typed entry buffer and a
  scalar open-addressed hash-index buffer. Both use the existing Heap, schemas,
  BufferBuilder/read leases and coordinator; no second allocator or collection
  framework. Stored hashes filter lookups, exact field/content/identity equality
  handles collisions, and native padding is ignored. Resizing stages both buffers,
  revalidates version/backing and commits last. Locked updates/removal/clear perform
  no fallible allocations after mutation begins. Removal compacts order; clear
  retains capacity and vacates traced references. Metadata contains no JIT callbacks.
- Shared LLVM lowering uses ordinary supported native ABI calls and target
  DataLayout geometry. Precise roots protect receivers, native entry/key staging,
  old/new buffers, results and fixed-child snapshots across safepoints. Existing
  value copies, shared references, nullable/interface values, deep readonly, staged
  publication/failure cleanup and hot reload remain intact.
- Seven real .void fixtures and two production CTest suites exercise supported key
  families, all existing value families/generic instances, cycles, nested values,
  independent repeated defaults, exact embedded-NUL strings, collisions, aliases,
  evaluation/capture order, iterator completion invalidation, readonly errors,
  missing-key failures, allocation rollback/retry, reference reclamation, compatible
  reload/old leases and data surviving complete JIT/analysis retirement. A fresh
  generation subsequently executes against the same Runtime. The large fixture
  stores twelve independent 2 MiB fixed payloads and iterates them without placing
  payloads on the native stack.
- Increased only `.github/workflows/ci.yml` timeout from 120 to **180 minutes**.
  Parsed old/new YAML with PyYAML BaseLoader, changed that single old value in
  memory and asserted complete structural equality, plus seven matrix entries.
  LLVM/cache, triggers/permissions, sanitizers/leaks, unfiltered CTest, editor and
  artifact steps are preserved. Evidence: build/stage3/workflow-validation.log.

### Failures investigated and corrected

Focused runs exposed incomplete new dictionary integration, not a need to rebuild
the existing Runtime: dictionary count initially entered ordinary field capability
projection; generic header/inference traversal initially only recognized arrays;
private entry shapes needed canonical structural ownership rather than duplicate
nominal definitions; and repeated dictionary defaults initially produced a nonpure
load prototype. The final constructor is one typed dictionary-new operation with
entry arguments, using the existing pure-template factory/outlining mechanism;
arbitrary mutation does not become a pure prototype. Receiver capture now precedes
key-side rebinding. Dictionary constant tooling metadata no longer advertises scalar
bits or nonexistent builtin source symbols. Positive/negative regressions cover
these paths.

A native sweep fixture initially passed an unregistered temporary entry reference
to append_native. GDB stopped at Runtime::require_output and identified its exact
word; the fixture now registers whole entry staging with Frame/Slot. The Runtime
root assertion was retained. Invalid native dictionary geometry is rejected before
publication. The fixture also retained source variable shadowing diagnostics; its
conflicting local/type name was renamed rather than relaxing that rule.

### Focused evidence

```sh
cmake --build build/order1/production --target voidc void_dictionaries_tests void_arrays_source void_generics_tests void_collections_tests -j3
ctest --test-dir build/order1/production -R '^(dictionaries_source|arrays_source|generics_source|collections_source)$' --output-on-failure -j2
ctest --test-dir build/order1/production -R '^(dictionaries_source|dictionaries_cli)$' --output-on-failure -j2
```

Adjacent suites: **4/4 passed**, 68.16 s. Feature source/CLI: **2/2 passed**,
49.17 s; the final tooling-metadata regression source rerun passed in 48.76 s.
The final CLI performs **92 executions**: 68 successful and 24 checked failures across
normal/O2 JIT and independently linked AOT. Explicit source tests preserve RHS
side effects after invalidation, verify unchanged selected values, and cover
structural change on the final iterator element.

On the focused Linux build, source allocation sweeps report **740** failing budgets
for main and **1,816** for the reference/fixed-value transaction fixture in each
codegen mode. Every failure checks valid balanced roots/publication, collection to
an empty heap and successful retry. Native sweeps report construction 20, growth
45, in-capacity insertion 10, replacement 10, removal 9, clear 9 and query 9 failures;
only genuinely allocation-required paths require a positive count. Header identity,
count/version, collision-chain elements and collection/cleanup remain asserted.
Final matrix evidence follows below; logs are under ignored build/stage3/.

During the final readiness review, an additional isolated real program reproduced
`VOID IR: invalid dictionary type`: `func entry(value: i32) -> i32: return
{1: 2}.count as i32` had no collection local/signature or string to activate the
coordinated Runtime. Dictionary construction now marks coordination at its shared
IR emission point, covering literal-only and numeric template-only programs. A
standalone literal fixture exercises both paths; final consolidated verification
is on this corrected source. Superseded build/test logs do not serve as final
acceptance evidence.

The first consolidated run also caught the existing scalar-iteration negative test's
exact message expectation: the new diagnostic correctly lists dictionaries among
iterable types. Updated only that expected text; scalar iteration must still reject
at 2:5 with the same diagnostic path. No acceptance condition or source assertion
was removed.

The Runtime-activation and diagnostic repairs passed the focused dictionary/frontend
suites **2/2** in 54.19 s; the isolated literal program returns 9 for input 7.
Final source is frozen before the corrected consolidated builds below.

### Final consolidated commands

```sh
cmake -S . -B build/order1/production
cmake --build build/order1/production -j3
ctest --test-dir build/order1/production --output-on-failure -j2
cmake -S . -B build/ci-release -DVOID_LLVM_RESEARCH=OFF
cmake --build build/ci-release -j3
ctest --test-dir build/ci-release --output-on-failure -j2
export ASAN_OPTIONS=detect_leaks=1:halt_on_error=1:exitcode=87
export UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1:exitcode=86
cmake -S . -B build/ci-sanitizers -DVOID_LLVM_RESEARCH=OFF
cmake --build build/ci-sanitizers -j3
ctest --test-dir build/ci-sanitizers --output-on-failure -j2
```

All configurations use LLVM 19.1.7 and research OFF. Sanitizer build/tests run
outside the tracing sandbox so LeakSanitizer can operate, with no suppressions.
Final evidence uses the `build/stage3/final-*-build.log` and `final-*-tests.log`
files; earlier superseded logs preserve the investigations above.

Small-stack commands for Debug and Release:

```sh
for configuration in order1/production ci-release; do
  (ulimit -s 1024; ctest --test-dir build/$configuration -R '^frontend$' --output-on-failure)
  python3 tests/integration/semantic_depth.py build/$configuration/voidc
  (ulimit -s 1024; build/$configuration/voidc run examples/dictionaries/large.void 7 --no-opt && build/$configuration/voidc run examples/dictionaries/large.void 7 && build/$configuration/void_dictionaries_cli_large_normal 7 && build/$configuration/void_dictionaries_cli_large_optimized 7)
done
```

Editor/package commands (Node 24.19.0 on PATH):

```sh
npm --prefix editors/vscode test
VOIDLS_PATH=/home/shmellyorc/Documents/Void.Language/build/order1/production/voidls VSCODE_EXECUTABLE_PATH=/usr/share/code/code npm --prefix editors/vscode run test:host
npm --prefix editors/vscode run package
```

Extension unit/typecheck/build, actual trusted/restricted/multi-root editor runs and
VSIX packaging pass. Artifact: editors/vscode/dist/void-language-0.1.0.vsix. Logs:
build/stage3/editor-tests.log, editor-host.log and vsix.log. Existing unfiltered CTest
covers LSP, native packaging/linkage/entry resolution, GC coordination, code/metadata
retirement and the full earlier array/generic/collection regressions.

### Documentation handoff

DEVELOPMENT_STRATEGY.md now owns one current implementation-status summary, with
Phases 1–25, accepted commit/owner-reported CI evidence, original Stage 0–7 status,
architecture, actual executable features, future design and meaningful limitations.
WRENCH_CLANKER.md leads with this summary and Work-first/minimal-output/owner-Git
rules before historical material. README, LANGUAGE_BIBLE, DECISIONS, KNOWN_ISSUES,
GC_RUNTIME and all active feature/tooling guides link that owner; dated proposals
and superseded research/activation gates are explicitly historical. DICTIONARIES.md
defines the new implemented contract. Historical results and unresolved findings
were preserved, not relabeled as new verification or erased.

### Final results and readiness

| Configuration | Production CTest | Elapsed |
| --- | --- | --- |
| Linux Debug | **74/74 passed** | 307.47 s |
| Linux Release | **74/74 passed** | 145.23 s |
| Strict ASan/UBSan + leak detection | **74/74 passed**, no sanitizer/leak findings | 301.26 s |

Successful-test details and allocation counts are preserved in
build/stage3/final-{debug,release,sanitizer}-details.log. All three configurations
report the same source/native dictionary sweep counts above: **5,224 intentional
allocation failures per configuration**, followed by successful budgets and retries.
The final seven-fixture CLI suite passes its 92 normal/O2 JIT/linked AOT executions.
Dictionary source tests include literal-only Runtime activation, nullable-present
values, generic value keys, live retired graphs and fresh-generation execution.

Debug and Release frontend/deep-receiver regressions pass under a **1 MiB stack**.
The nested 2 MiB dictionary payload fixture returns 14 in all four normal/O2
JIT/linked AOT modes under that stack in both configurations (eight executions).
Evidence: build/stage3/debug-stack.log and release-stack.log. Real editor trusted,
restricted and multi-root runs, extension tests/typechecking and VSIX packaging
pass on the corrected compiler; no client functionality or checks were disabled.

Stage 3 is **locally complete and ready for owner-controlled hosted acceptance**
for its implemented managed-memory contract: one allocator/precise coordinated GC,
reference/value structs, immutable strings, fixed/growable lists and ordered
dictionaries, correct under normal/O2 native JIT/AOT. This is not a claim of a new
production GC certification, concurrent collection or completion of Stages 4–7.
Phases 1–25 and Orders 1–5 remain intact. CI timeout is verified at **180 minutes**,
with the seven-job workflow otherwise structurally unchanged.

Remaining meaningful boundaries: result-based dictionary get/pop, float/interface/
collection/custom-equality keys, string indexing/interpolation, omitted slice
endpoints/views, source concurrency/general FFI and other future language/tooling
work remain deferred. Dictionary deletion is O(n); root lifetimes remain conservative.
Dictionary lookup/count/methods are runtime operations, even for pure template data.
The pre-existing deep valid receiver-chain lowering stack issue remains documented
in KNOWN_ISSUES.md; no unrelated repair is claimed. There are no failing final local
production tests or sanitizer findings. Windows/macOS acceptance of this new source
remains pending the owner's hosted run.

`git diff --check` passes. HEAD remains c946510a0a72428d7d5849007f8328753377cdf6;
all delivery edits are uncommitted. No branches, commits, pushes, remote changes
or hosted CI triggers were performed.

## 18 October 2026: Stage 3 hosted lock; Stage 4 contracts and native accessor/callable subset

Baseline HEAD verified at `cd95e37341f756e749ea5f3d6da9ad44716d491f`.
The owner's [CI #65](https://github.com/Shmellyorc/Void.Language/actions/runs/39125218619)
confirmation locks Stage 3 Managed Runtime, dictionaries and integration across
all seven jobs. This is owner-provided hosted evidence. The pre-existing uncommitted
Stage 5 persistent Debug cache design in DEVELOPMENT_STRATEGY.md is preserved.
No commits, branches, pushes, remote changes or hosted CI actions were performed.

Before implementation, recorded the approved Stage 4 contracts in Bible Section
0.3 and DECISIONS.md. The owner additionally approved explicit `var` pattern
captures (bare names compare), managed-only no-result one-shot deinit, and explicit
fallible factories without init rewriting. These remain **design**, not an
implementation claim. Exact result signatures/error types for dictionary get and
list pop are still deferred.

**Actual implementation delivered:** Bible property/get/set declarations on
managed/value/concrete generic receivers, field-like reads and assignment/compound
assignment, nested value/fixed-array update through setters, inherited and one-hop
using selection, module visibility, deep readonly checks, and nonmutating getters
including transitive receiver effects. Accessors are ordinary methods in the
existing semantic checker, typed IR, LLVM lowerer and Runtime; no new backend,
Runtime operation, ownership registry or alternative execution engine is added.
Accessor role is validated in typed IR and included in reload compatibility.
Body replacement and old-code leases are exercised; setter removal requires restart.

Concrete structs/interfaces with compatible call methods can be invoked through
ordinary callable syntax, including locals, fields, managed indexed elements,
properties and returned temporaries. Generic functions can receive concrete
callable structs. Normal argument/result, receiver, visibility, readonly and GC
rules apply. This is **not function types/values or callback erasure**.
PROPERTIES.md gives the executable scope and limitations. Tooling dependencies
include accessors, and the existing TextMate grammar recognizes property/get/set.

Corrections during integration:

- An experimental selected-inline-element copy-back path was unsafe: in
  build/stage4/array-receiver-alias.void a callee changed a sibling field through
  an array alias to 99, then copy-back overwrote it with 0. Removed that extension;
  indexed inline mutating receivers remain rejected. The original arrays_source
  rejection assertion is unchanged (tests/arrays/source.cpp has no delivery diff).
  A new callable regression checks this rejection too. Managed indexed callables
  retain identity and need no inline copy-back. No mutable element borrow is added.
- Computed value-property receivers must evaluate their getter before RHS effects.
  The receiver fixture checks a failing getter cannot be rescued by an RHS which
  changes its denominator. This applies to normal/O2 JIT and linked AOT.
- A nonmutating inner setter must not publish an unchanged captured outer value
  over RHS effects. Copy-back is conditional on the inferred setter receiver
  effect; the receiver fixture preserves the RHS value 99. No-op setters still
  require readonly authority to be respected.
- Kept ordinary direct field assignment on its existing initialization path;
  only property-containing paths enter selected-property lowering. An early broad
  rewrite produced mistyped managed-init IR and was corrected before acceptance.
- The first consolidated Release pass reproduced a composition_source diagnostic
  regression: a scalar field hiding a promoted method reported "Member access
  requires a struct" instead of the established not-callable error. Callable-field
  adaptation now rejects scalar fields before receiver rewriting. The existing
  composition assertion is preserved and the corrected-source pass follows below.
- Allocation-failure test probes reserve their own frame-observation storage
  before injection, matching the existing test convention; injected failures must
  not abort a noexcept observer through the test's own vector allocation.

**Stage 4 is incomplete.** Enums, shared is/match patterns, result/Ok/Err/?,
defer/with/deinit execution, function types/values, anonymous/escaping closures,
events and erased source callbacks remain unimplemented. All their approved
contracts are retained. This delivery does not fulfill the complete Stage 4 mission
or authorize a Stage 4 hosted lock. Mutating inline array/dictionary element
receivers still require an independent local value and explicit replacement.
The pre-existing deep valid receiver-chain stack limitation remains in KNOWN_ISSUES.md.

Validation commands and final results follow below after the corrected-source pass.

Reproducible commands (repository root, existing build configurations; all LLVM
research options OFF, `/usr/bin/clang++-19`, LLVM 19.1.7 at `/usr/lib/llvm-19`):

```sh
cmake --build build/order1/production -j 2
ctest --test-dir build/order1/production --output-on-failure
cmake --build build/ci-release -j 2
ctest --test-dir build/ci-release --output-on-failure
ASAN_OPTIONS=detect_leaks=1:halt_on_error=1:exitcode=87 \
UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1:exitcode=86 \
cmake --build build/ci-sanitizers -j 2
ASAN_OPTIONS=detect_leaks=1:halt_on_error=1:exitcode=87 \
UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1:exitcode=86 \
ctest --test-dir build/ci-sanitizers --output-on-failure
```

Sanitizer build generators and tests run outside the tracing sandbox so LSan is
fully enabled; no sanitizer checks or production tests are disabled. Debug is
CMAKE_BUILD_TYPE=Debug, Release is Release; sanitizers use RelWithDebInfo and
VOID_SANITIZERS=ON. The existing `properties_source`/`properties_cli` additions
bring the 74-suite baseline to 76. The seven-job workflow and 180-minute timeout
are unchanged.

Small-stack and malformed/deep receiver checks:

```sh
(ulimit -s 1024; ctest --test-dir build/order1/production -R '^frontend$' --output-on-failure)
python3 tests/integration/semantic_depth.py build/order1/production/voidc
(ulimit -s 1024; build/ci-release/void_tests frontend /home/shmellyorc/Documents/Void.Language)
python3 tests/integration/semantic_depth.py build/ci-release/voidc
```

For each `B` equal to `build/order1/production` and `build/ci-release`, the four
2 MiB property-array executions also run under `ulimit -s 1024`:

```sh
"$B/voidc" run examples/properties/references.void 7 --no-opt
"$B/voidc" run examples/properties/references.void 7
"$B/void_properties_cli_references_normal" 7
"$B/void_properties_cli_references_optimized" 7
```

Editor commands (Node 24.19.0 on PATH):

```sh
npm --prefix editors/vscode test
VOIDLS_PATH=/home/shmellyorc/Documents/Void.Language/build/order1/production/voidls \
VSCODE_EXECUTABLE_PATH=/usr/share/code/code npm --prefix editors/vscode run test:host
npm --prefix editors/vscode run package
```

The real editor test uses the local display/IPC outside the sandbox. VSIX output
is `editors/vscode/dist/void-language-0.1.0.vsix`. Logs are under the ignored
`build/stage4/` directory: `{debug,release,sanitizer}-{build,tests}.log`,
`{debug,release}-stack.log`, `editor-unit.log`, `editor-host.log` and `vsix.log`.

### Final corrected-source local results

| Configuration | Production CTest | Wall time |
| --- | --- | --- |
| Linux Debug | **76/76 passed** | 625.12 s |
| Linux Release | **76/76 passed** | 254.07 s |
| Linux strict ASan/UBSan/leaks | **76/76 passed** | 618.62 s |

All 74 pre-existing suites remain present, with assertions preserved, alongside
properties_source and properties_cli. The final runs execute the corrected scalar
callable diagnostic. The three configurations ran concurrently; wall times are
validation timings, not performance benchmarks. No final failing tests or sanitizer
findings remain for the implemented subset.

Each configuration reports 172 intentional property allocation failures in normal
JIT and 172 in O2 JIT, followed by success, balanced precise roots, collection and
successful retry. Native accessor/callable integration reports **44 successes and
12 source-located checked failures** through normal/O2 JIT and independently
linked AOT. Failure budgets are test-only and not production limits. The existing
GC, callbacks/quiescence, hot-reload/retirement, dictionary, array, generics and
publication/failure suites pass without weakening their assertions.

Final 1 MiB checks pass in Debug and Release: both frontend suites and bounded
malformed/deep-chain scripts, plus all eight normal/O2 JIT/linked AOT executions
of the 2 MiB fixed-array property fixture (each returns 12). The final exact command
uses `void_tests frontend /home/shmellyorc/Documents/Void.Language` under the stack
limit for **both** build directories; full evidence is build/stage4/final-stack.log.
The known separate valid deeply chained receiver limitation is not claimed fixed.

Extension unit tests/typechecking, all three real editor modes (trusted, restricted,
multi-root), and VSIX packaging pass. Full per-test output, including allocation
counts, is saved in build/stage4/{debug,release,sanitizer}-details.log.
`git diff --check` passes; HEAD remains cd95e373. The original array mutation rejection
and ci.yml are unchanged. No commits, branches, pushes, remote changes or hosted
CI triggers were made.

Source changes are in src/frontend/compiler.cpp (property parsing/selection and
callable syntax), capabilities.inc (shared call/write effects), generics.inc
(accessor specialization roles), include/void/ir.hpp and src/ir/{module.cpp,
interfaces.inc} (accessor role/signature/conformance checks). The LLVM backend and
production Runtime are unchanged. Tests use the existing CMake/CTest source and
native-link infrastructure: tests/properties/tests.cpp,
tests/integration/properties.cmake and examples/properties/*.void. The existing
VS Code grammar/config tests cover accessor highlighting. PROPERTIES.md documents
the local feature boundary; canonical status, Bible/decisions, known issues,
receiver guide, README and WRENCH handoff are reconciled.

The implemented subset is locally validated and available for owner review.
**The full Stage 4 mission is not complete or ready for Stage 4 acceptance.**
Remaining enums, result/patterns, defer/with/deinit, function values/escaping closures,
events and erased source callbacks are approved but unimplemented. No missing
feature is represented by a stub, alternate backend or claimed working syntax.
Stage 3 stays hosted-locked; Stage 4 has no hosted lock. Stage 5's persistent Debug
cache/automatic watching remains approved future design, untouched by production
changes here.

## 18 October 2026: Stage 4 continuation, complete advanced source systems

Resumed the existing uncommitted properties/callable implementation; Stage 3 stays
locked at cd95e373 / owner CI #65. The continuation implements tagged/data-carrying
enums, contextual result<t,e>/Ok/Err and exact-error propagation; shared is/match
patterns with explicit success-scoped var captures; first-class functions,
anonymous/escaping closures, generic/method/interface/callable-struct callbacks;
defer, with, managed-only one-shot deinit; and synchronous ordered events.
ADVANCED_LANGUAGE.md documents executable syntax, authority/lifetime rules and
boundaries. Properties/callable structs remain intact.

The existing semantic checker, typed IR and shared LLVM 19.1.7 lowerer implement
all execution. Sum payloads occupy disjoint zeroed fields and trace only valid
references. Closures use ordinary managed cells/environments with precise roots;
value receivers snapshot independently, ordinary captured locals share cells.
No executable pointer is stored in managed metadata. Callback dispatch and reload
use the existing generation lease. Events reuse growable buffers and snapshot
subscribers before synchronous calls. Defer nodes are ordinary managed objects;
with prepares cleanup storage before acquisition. Explicit/deferred/with release
shares one synchronized object bit. Runtime failures drain applicable cleanup,
preserve the primary error and attach bounded additional diagnostics.

Owner decisions added to DECISIONS.md: deferred call arguments/receiver evaluate
at registration; deferred blocks read shared state at cleanup; escaping captured
value receivers snapshot independently, shared by copies of their closure.

Integration defects reproduced and corrected:

- New expression cases increased recursive semantic stack frames and broke the
  existing 1 MiB deep-receiver rejection. GDB identified the dispatcher frame;
  extracted handlers using the existing semantic-handler convention. Kept depth
  guards/assertions unchanged. Added deep closure/defer recovery to the existing
  semantic_depth.py regression, without a second harness.
- Capture authority profiles now join monotonically; readonly retention exemptions
  apply to the destination field schema, never the source expression's authority.
  Consolidation exposed otherwise-accepted readonly storage in managed/value/
  interface construction; all original negative tests remain and now reject.
- Function environments share a stable private builtin base across independently
  compiled roots using the same Runtime. Capture-layout/authority changes enter
  the compatibility contract and require restart before Runtime admission.
- Managed/interface bound callback identities compare underlying receiver identity;
  virtual/base and readonly aliases canonicalize to the same adapter. Compatible
  body reload and old leased code retain their behavior.
- Multiline closures/matches inside dictionary literals restore lexer indentation
  context. Constructor escape checking distinguishes registered expression
  snapshots from captured constructor storage. Explicit complete result/function
  field defaults satisfy the existing value-init completeness rule.
- Pattern condition checking preserves the existing lazy numeric diagnostics in
  false-and/true-or branches. Invalid interface field reads retain the established
  diagnostic while valid interface method references remain callable.
- Pattern CFG block numbering exposed quadratic initialization-dataflow convergence:
  modules_cli exceeded its unchanged 180-second limit in Debug and Release. GDB
  sampled set intersections in validate_reference_initialization. Its existing
  fixed-point checker now visits reverse postorder (iteratively, avoiding native
  recursion), preserving every initialization check. The same 304-case numeric
  matrix checked in 3.473 seconds in corrected Debug; focused modules_cli passed
  in 54.50 seconds. No timeout/test/sanitizer was weakened.

Focused corrected commands/results:

```
cmake --build build/order1/production -j2 --target voidc void_managed_tests void_managed_initialization_tests void_reference_values_tests void_interfaces_tests void_tests
ctest --test-dir build/order1/production -R '^(managed_source|managed_initialization_source|reference_values_source|interfaces_source|numeric)$' --output-on-failure -j1
# 5/5 passed; build/stage4/corrections-focused.log
cmake --build build/order1/production -j2 --target voidc void_advanced_tests void_patterns_tests void_tests
ctest --test-dir build/order1/production -R '^(advanced_source|patterns_source|numeric|modules_cli|numeric_cli)$' --output-on-failure -j2
# 5/5 passed; build/stage4/final-corrections-focused.log
```

The inventory expands from 76 to 80 production suites: patterns_source/cli and
advanced_source/cli. Fourteen pattern and twenty-two advanced fixtures execute
through normal/O2 JIT and independent native linking. Source suites force GC before
construction, inspect precise roots/publication, sweep allocation failures, check
retries and reclamation, primary/secondary cleanup failures, with-acquisition
failure, compatible reload and incompatible capture-layout restart. Combined
programs use properties, generics, collections, results, patterns, cleanup,
closures, callbacks and events together. The 2 MiB capture fixture executes with a
1 MiB stack in both JIT modes and both separately linked native modes.

Final corrected consolidated validation (LLVM 19.1.7, research OFF):

| Configuration | Production CTest | Elapsed |
| --- | --- | --- |
| Debug | 80/80 passed | 400.47 s |
| Release | 80/80 passed | 164.27 s |
| RelWithDebInfo, strict ASan/UBSan/leaks | 80/80 passed | 358.68 s |

```
cmake --build build/order1/production -j2
ctest --test-dir build/order1/production --output-on-failure -j2
cmake --build build/ci-release -j2
ctest --test-dir build/ci-release --output-on-failure -j2
ASAN_OPTIONS=detect_leaks=1:halt_on_error=1:exitcode=87 UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1:exitcode=86 cmake --build build/ci-sanitizers -j2
ASAN_OPTIONS=detect_leaks=1:halt_on_error=1:exitcode=87 UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1:exitcode=86 ctest --test-dir build/ci-sanitizers --output-on-failure -j2
```

Sanitizer builds/tests ran outside the tracing sandbox so LeakSanitizer could
operate normally; leak detection stayed enabled, including generated-object
compilation. Logs: build/stage4/{debug,release,sanitizers}-final-{build,ctest}.log;
full per-test output is also in each build's Testing/Temporary/LastTest.log.
All original 76 suites remain. Allocation sweeps in each configuration and each
JIT mode observe captures 232, with 230, events 412 and fixed_capture 133 failures;
patterns observe values 418, large 232 and combined 169 failures. Cleanup, roots,
transactional publication, retry and final reclamation assertions all pass.

For each of the three build directories, ran the following under `ulimit -s 1024`
(with the same strict sanitizer environment for ci-sanitizers):

```
BUILD/voidc run examples/advanced/large.void 7 --no-opt
BUILD/voidc run examples/advanced/large.void 7
BUILD/void_advanced_cli_large_normal 7
BUILD/void_advanced_cli_large_optimized 7
BUILD/void_tests frontend /home/shmellyorc/Documents/Void.Language
```

Each of the twelve large-payload executions returned 17; all three complete
frontend runs passed. Logs: build/stage4/final-small-stack-{debug,release,sanitizers}.log.
The full CTest diagnostics workflow also runs the extended semantic-depth recovery
script with its existing 1 MiB child stack limit. Scratch diagnostics generated
by the standalone frontend runs were moved under ignored build/stage4.

```
npm --prefix editors/vscode test
npm --prefix editors/vscode run package
PATH=/home/shmellyorc/.nvm/versions/node/v24.19.0/bin:$PATH VOIDLS_PATH=/home/shmellyorc/Documents/Void.Language/build/order1/production/voidls VSCODE_EXECUTABLE_PATH=/usr/share/code/code npm --prefix editors/vscode run test:host
```

Extension typechecking/unit tests: 1/1 passed. Real VS Code trusted, restricted
trust and multi-root modes: all passed against the final corrected voidls.
VSIX: editors/vscode/dist/void-language-0.1.0.vsix (8 files, 145.71 KB reported
by vsce). Logs: editor-final-tests.log, editor-final-package.log and
editor-corrected-host.log under build/stage4. LSP/native/session/reload regressions
also pass in each full CTest run. The existing seven-job workflow retains
unfiltered production CTest, frontend stack checks, real editor integration and
VSIX packaging; ci.yml and its 180-minute job timeout are unchanged.

Actual implementation files: frontend compiler.cpp plus pattern_parser.inc,
pattern_semantics.inc and pattern_lowering.inc, shared capabilities/construction/
generic helpers; include/void/{ir,frontend,diagnostic,reference_capabilities}.hpp;
src/ir/module.cpp, reference_capabilities.inc and interfaces.inc; the shared LLVM lowerer;
and the existing Runtime/Heap ABI/schema/array/coordination files. Source fixtures
and regression suites use the existing CMake native-link and managed probe helpers.
No alternate checker, execution engine, GC or ownership registry was introduced.
Feature/status/Bible/decisions/diagnostics/generic/receiver/array/interface/inheritance
guides, known issues and Wrench handoff are reconciled. `git diff --check` passes;
HEAD remains cd95e373. Stage 4 is ready for owner review and hosted CI verification;
no new cross-platform or hosted acceptance is claimed.

Remaining boundaries: no generic enum declarations, unit/void result payload
syntax, module-time pattern execution or bound-receiver allocation, arbitrary
managed/native callback FFI, async or automatic watching/persistent Debug cache.
Callback readonly result authority is conservative. Dictionary get/list pop still
await approved exact signatures/error types. Root lifetimes remain conservative;
the separately documented deeply valid receiver-chain lowering limit remains.
No hosted platform is claimed validated by this Linux sprint. Stage 5's future
persistent Debug cache proposal is preserved. No Git/remote/hosted CI actions.

## 18 October 2026: Windows CI #66, move-only Pattern relocation

Baseline HEAD: f904c34f82957b6dbd98e63af31848cd913c3216; clean worktree before
this correction. The owner supplied the Windows Debug/Release compiler failure
from [CI #66](https://github.com/Shmellyorc/Void.Language/actions/runs/39162980962):
xmemory construct_at, Pattern vector relocation, copying values, then deleted
unique_ptr<Expr> copy. No Windows compiler/STL execution is claimed locally.

Cause: Pattern's implicitly declared copy operations appear available to type
traits because vector declares copying without instantiating its element-copy
body. When relocation chooses that copy path rather than a potentially throwing
move, the owned expressions cannot be copied. Expr has the same latent contract
through its owning vectors and enclosed patterns. This is the same ownership
boundary as the previous Structure correction. Microsoft's explanations of
[vector copy traits](https://devblogs.microsoft.com/oldnewthing/20190926-00/?p=102924)
and [move_if_noexcept](https://learn.microsoft.com/en-us/cpp/standard-library/utility-functions?view=msvc-170#move_if_noexcept)
confirm why a declared-but-uninstantiable copy can select the wrong path.

Reproduction on Linux/Clang 19: four assertions requiring Pattern/Expr to be
noncopyable fail on the baseline. Explicitly instantiating the baseline
vector<Pattern> copy reproduces the construct_at/deleted unique_ptr<Expr> chain
at compiler.cpp:237. Linux ordinarily selects moving, so this reproduces the
incorrect trait contract and failing copy body, not the Windows STL selection.
Scratch probes and diagnostics are in build/windows-pattern-relocation:
baseline-ownership.log, baseline-copy.log and corrected-ownership.log. Commands:

```
clang++-19 -std=c++20 -I. -Iinclude -fsyntax-only build/windows-pattern-relocation/ownership.cpp
# Baseline: four failed noncopyability assertions. Corrected source: passes.
clang++-19 -std=c++20 -Iinclude -Isrc/frontend -fsyntax-only build/windows-pattern-relocation/baseline-copy.cpp
# Expected negative reproduction from the original compiler.cpp plus explicit copy.
```

Small production correction: Pattern and Expr explicitly delete copy construction/
assignment and default move construction/assignment. Pattern retains its default
constructor. No forced noexcept annotation: exception specifications are inferred
from the actual members. Compile-time assertions require noncopyability, movable
construction/assignment and rvalue selection by move_if_noexcept. Enclosing
Statement/Function movement and noncopyability are checked; Structure's existing
ownership assertions remain. No parser, language, LLVM, Runtime/GC or CI behavior
is changed.

Added examples/patterns/relocation.void to the existing pattern source/native
fixtures. It grows nested child vectors, dictionary key-expression vectors,
constructor field patterns, range-valued match-expression arms and statement arms
owning optional Patterns. First/middle/last/fallback inputs 0/7/8/99 must produce
134/196/240/456 in normal/O2 JIT; the shared pattern CLI suite also verifies the
fixture through normal/O2 JIT and independently linked AOT. Original assertions
and all existing fixtures remain intact. No new test framework or suite is added.

Focused verification first: rebuilt voidc/void_patterns_tests, then
`ctest --test-dir build/order1/production -R '^patterns_source$' --output-on-failure`:
1/1 passed (7.21 s), including the relocation fixture, GC, failure sweeps and reload.

Expanded focused validation rebuilds affected frontend/tooling binaries, source
suites and the existing pattern/advanced/property native fixture targets. It
runs the following unchanged CTest families, without the unrelated full matrix:

```
ctest --test-dir BUILD -R '^(frontend|modules|numeric|tooling|analysis|lsp_positions|lsp_protocol|patterns_source|patterns_cli|advanced_source|advanced_cli|properties_source|properties_cli|source_cli|analysis_cli)$' --output-on-failure -j2
```

Final focused results with LLVM 19.1.7 and research OFF:

| Configuration | Focused CTest | Elapsed |
| --- | --- | --- |
| Debug | 15/15 passed | 82.92 s |
| Release | 15/15 passed | 20.13 s |
| Strict ASan/UBSan/leaks (RelWithDebInfo) | 15/15 passed | 57.21 s |

Exact local orchestration commands (the disposable helper configures each cached
build and selects the affected existing CMake targets, not a new test framework):

```
python3 build/windows-pattern-relocation/verify.py build/order1/production debug
python3 build/windows-pattern-relocation/verify.py build/ci-release release
ASAN_OPTIONS=detect_leaks=1:halt_on_error=1:exitcode=87 UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1:exitcode=86 python3 build/windows-pattern-relocation/verify.py build/ci-sanitizers sanitizers
```

The expanded exact cmake build target lists and CTest commands are saved as
build/windows-pattern-relocation/{debug,release,sanitizers}-commands.txt; configure/
build and test output are in the corresponding -build.log and -tests.log files.
Sanitizer compilation, emitted-object construction and tests ran outside tracing
with leak detection enabled throughout. All 45 selected tests pass, including
normal/O2 JIT, independent native AOT, pattern allocation failures/GC/reload,
Stage 4 closures/events/cleanup/properties, parser recovery, owned analysis/tooling,
LSP and the existing semantic-depth 1 MiB stack checks through source_cli.
No failures were masked and no existing assertion or fixture was removed.

`git diff --check` passes. Source changes are limited to compiler.cpp special
members/assertions, the relocation source fixture and its existing CMake/source/
CLI test registration. LLVM, Runtime/GC, language semantics, sanitizer settings
and ci.yml are unchanged. A full 80-suite matrix was not repeated for this AST
ownership-only correction. Windows Debug/Release compilation and hosted regression
acceptance remain for the owner's CI rerun; local Linux results do not substitute
for that platform verification. No commits, branches, pushes or hosted CI actions.

## 18 October 2026: CI #67 Windows Debug arrays_foundation crash

Inspected clean HEAD 1e6a66f109a0641127015b32204021a7f255b6f7. Downloaded the
actual failed [Windows Debug job](https://github.com/Shmellyorc/Void.Language/actions/runs/39171696075/job/117805914321)
log and tests-Windows-X64-Debug artifact (id 12152573039). The job used pinned
LLVM 19.1.7 clang-cl with Debug /Zi /Ob0 /Od /RTC1. It records arrays_foundation
SegFault after 0.07 s, with every other test passing. LastTest.log contains no
foundation output or native dump. Windows Release job 117805914291 succeeded.
Read-only retrieval commands:

```
gh run view 39171696075 --repo Shmellyorc/Void.Language --json jobs,headSha,conclusion
gh api repos/Shmellyorc/Void.Language/actions/runs/39171696075/artifacts
gh api repos/Shmellyorc/Void.Language/actions/jobs/117805914321/logs
gh run download 39171696075 --repo Shmellyorc/Void.Language --name tests-Windows-X64-Debug --dir build/windows-arrays-crash/windows-debug-artifact
```

The raw job log, original artifact and native investigation evidence remain under
ignored build/windows-arrays-crash. No hosted job was started or rerun.

**Demonstrated cause:** compiler parser stack exhaustion, not array payload stack
allocation, stale array storage or an IR/GC failure. After relinking the exact
current Debug source, running void_arrays_foundation with `ulimit -s 1024`
reproduced SIGSEGV. GDB entry breakpoints showed source_and_ir completed and
negatives entered. The crash is the existing foundation.cpp:142 rejection of a
160-level nested fixed-array annotation. Backtrace: negative -> analyze_source ->
Parser::parse/block/statement -> 87 recursive Parser::type frames. The faulting
instruction spills an argument below the stack after the type prologue reserves
0x2cd0 (11,472) bytes; it occurs before the next Depth guard can execute. The
parent frame has depth=87 and the current token is `[`. No semantic/IR validation
has begun for that annotation. See baseline-stack-current.log.

The Stage 3 foundation source is unchanged between cd95e373 / CI #65 and this
commit. Built that exact prior commit from a read-only git archive, separately
under build/windows-arrays-crash/stage3-source and stage3-debug, using the same
Clang 19, LLVM 19.1.7 and -g/no-optimization flags. Its complete foundation suite
passes at 1 MiB. Its Parser::type prologue reserves 0x1d60 (7,520) bytes.
Stage 4 added function/result/parenthesized type parsing to the same recursive
function; Debug reserves space for branch-local temporaries even when those
branches are not selected. This grew every nested array's native frame past the
existing guard's stack budget. This controlled reproduction explains the
Debug-only Windows failure without assuming that CTest's generic SegFault label
is itself a Windows exception-code diagnosis. Microsoft's documented x64 default
[stack reserve is 1 MiB](https://learn.microsoft.com/en-us/cpp/build/reference/stack-stack-allocations?view=msvc-170).

**Repair:** separate the existing function/result/dictionary/array/named type
productions into ordinary Parser handlers using the existing noinline handler
boundary. Recursive type() retains the same shared 128-level Depth guard, token
ordering and parentheses behavior. Branch-local storage is now paid only for the
selected production. Corrected Debug prologues reserve 0x430 (1,072) bytes for
type() and 0x620 (1,568) for array_type(), instead of 11,472 bytes for every level.
These are measured Linux Clang Debug frames, not claimed Windows measurements.
No grammar/type semantics, guard limits, LLVM version, Runtime/GC, stack reserves,
optimization flags or CI configuration changed.

All five unchanged foundation subtests now complete at 1 MiB, including negatives,
compact_shapes, indexes_and_budget and owned_ir. Added flushed subtest labels to
main so a future hosted crash artifact identifies the last entered section.
Existing semantic_depth.py now covers fixed/growable arrays, dictionaries,
function/result/generic/parenthesized and mixed type nesting: shallow forms must
compile; 160 levels must report exactly one VOID1107 nesting error and recover to
an independent later VOID2003 error. It uses the existing 1 MiB Linux child limit
and the normal Windows executable stack, without a new test harness or relaxed
assertions. The existing receiver/closure/defer depth checks remain intact.

First focused checks passed:

```
cmake --build build/order1/production -j2 --target void_arrays_foundation voidc
(ulimit -s 1024; build/order1/production/void_arrays_foundation)
python3 tests/integration/semantic_depth.py build/order1/production/voidc
```

Final affected verification (LLVM 19.1.7, research OFF):

| Configuration | Focused CTest | Elapsed | Foundation + depth recovery at 1 MiB |
| --- | --- | --- | --- |
| Debug | 22/22 passed | 170.23 s | passed |
| Release | 22/22 passed | 53.89 s | passed |
| Strict ASan/UBSan/leaks | 22/22 passed | 119.69 s | passed |

Exact focused CTest command, with BUILD set to build/order1/production,
build/ci-release and build/ci-sanitizers respectively:

```
ctest --test-dir BUILD -R '^(arrays_foundation|fixed_arrays_source|growable_arrays_source|arrays_source|generics_source|dictionaries_source|collections_source|frontend|modules|numeric|tooling|analysis|lsp_positions|lsp_protocol|patterns_source|patterns_cli|advanced_source|advanced_cli|properties_source|properties_cli|source_cli|analysis_cli)$' --output-on-failure -j2
```

Actual local orchestration commands:

```
python3 build/windows-arrays-crash/verify.py build/order1/production debug
python3 build/windows-arrays-crash/verify.py build/ci-release release
ASAN_OPTIONS=detect_leaks=1:halt_on_error=1:exitcode=87 UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1:exitcode=86 python3 build/windows-arrays-crash/verify.py build/ci-sanitizers sanitizers
```

The disposable helper rebuilds affected existing CMake targets and pattern/advanced/
property native fixtures, runs the selected suites, then runs the full foundation
binary at 1 MiB and the existing extended semantic-depth script. Expanded build
commands are saved in {debug,release,sanitizers}-commands.txt; build/test/small-stack
output is in the corresponding -build.log, -tests.log and -small-stack.log under
build/windows-arrays-crash. Sanitizer builds, emitted objects and tests run outside
tracing with the above strict environment and leak detection enabled throughout.
All 66 focused suites pass, including normal/O2 JIT, independently linked AOT,
large arrays, GC/failure cleanup, Stage 4 patterns/closures/events/results,
generics, owned tooling, reload and parser recovery. The entire unchanged
foundation suite, including compact shape/owned IR negatives, passes at 1 MiB in
all three configurations; no subtest or assertion was disabled.

The full unrelated 80-suite matrix was not repeated. Runtime/GC, LLVM, source
contracts, existing budgets, sanitizer settings and ci.yml are unchanged.
`git diff --check` passes. Source changes are limited to the type-production
handler boundaries, foundation progress labels and existing depth regression.
The actual Windows artifact supplies no native dump, so the reported native trace
and controlled before/after measurements are the equivalent Linux 1 MiB
reproduction. Windows Debug hosted confirmation remains for the owner's next CI
attempt; no new hosted pass is claimed. No commits, pushes, branches, remote
modifications or hosted CI actions were made.

## 18 October 2026: Stage 4 hosted lock and Stage 5 cache architecture investigation

Baseline verified at `483e67e407b65674bdc82d44767f29bd83d73ae8`, initially
clean. Stage 4 is **hosted-verified and LOCKED** at
[CI #68](https://github.com/Shmellyorc/Void.Language/actions/runs/39174408997):
owner reports 80/80 suites in each of seven configurations (560/560 total).
Read-only run metadata independently confirms this commit and all seven successes.
Canonical status, Bible, decisions, feature guides, known issues and Wrench handoff
now record that acceptance. Earlier pending-hosted entries remain historical.

Delivered `docs/STAGE5_CACHE_PROPOSAL.md`, **awaiting owner approval**. Production
compiler, Runtime, tests, CI and Git state were not changed. No persistent cache,
watcher, new language behavior, backend, verifier or dependency system was added.
LLVM 19.1.7 remains pinned; Release AOT remains unchanged.

### Investigation and recommendation

Inspected SourceGraph/disk consistency, full semantic/generic validation,
ValidatedAnalysis snapshots/contracts, ToolingSession/plan_invalidation, whole-graph
typed IR, shared LLVM lowering, native object materialization, Runtime metadata
admission and generation leases/retirement. Started with official LLVM 19 ORCv2
reference; checked exact installed 19.1.7 headers and downloaded tagged
CompileUtils.cpp, LLJIT.cpp and Unix/Windows filesystem implementations.

Recommend one relocatable object per reachable source graph/selected entry. Full
current-source checking and existing entry checks precede a hit; exact matching
inputs reuse the previous typed-IR/LLVM-validated native result without rebuilding
IR. Misses retain every existing IR/LLVM check. One JITDylib/ResourceTracker per
generation isolates stable object symbols. Reuse the object's existing type and
witness catalogs through vd_runtime_admit_metadata, with current Runtime identity,
source contracts and NativeCode leases. Whole-unit first-time edits still compile
cold; finer partitioning needs real native boundaries, not another dependency graph.

The proposal specifies content/build/target identity, private local storage,
single-file length/digest validation, no-replace LLVM hard-link publication,
interruption/concurrent-process handling, normal compilation fallback and required
regressions. A build-derived embedded compiler ID is justified by the measured
234 ms cost of hashing the 59.7 MB compiler each launch. It must derive from actual
CMake link inputs/configuration and requires implementation/rebuild tests; no stale
sidecar or timestamp shortcut is proposed. Hard-link-unsupported filesystems skip
persistent writes; damaged existing entries miss until this disposable cache is
cleared. Cross-platform cache behavior and actual reboot/power loss are unverified.

### Measurements and executable evidence

Ignored research sources/scripts/objects/logs are retained under
`build/stage5-cache-research/`; no experimental source was put in production.
Scratch copies added scoped timers to the actual compiler/lowerer and used LLVM's
ordinary compile/object layers. The load probe takes an explicitly named trusted
object: it is not a production cache or an implementation of identity/fallback.

Linux x86-64 Ryzen 7 1700X / 32 GiB, Clang/LLVM/LLD 19.1.7; host Release
`-O3 -DNDEBUG -frtti`, guest normal Debug/no middle-end O2, research OFF.
Three fresh processes per mode, warm OS pages, main-to-first-verified-result medians:

| Source workload | Cold | Reuse retaining current typed IR | Reuse after full current source validation |
| --- | ---: | ---: | ---: |
| Real Stage 4 combined | 0.1701 s | 0.01293 s | 0.00357 s |
| 2,223 generated lines | 1.894 s | 0.2110 s | 0.04256 s |
| 21,243 generated lines | 19.931 s | 2.836 s | 0.4824 s |
| 105,243 generated lines | 142.888 s | 41.095 s | 5.694 s |

Largest fixture: 21 reachable modules, 5,000 executed mixed-feature helpers,
5,041 concrete functions, 2,380,496 source bytes; generated scaling workload,
not an existing 100k-line application. Cold range 140.554–143.853 s; direct native
reuse 3.653–5.833 s. Native object 51,530,744 bytes. Phase medians at 105k:
analysis 2.761 s (read 12.72 ms, lex 124.37 ms, parse 260.76 ms, semantic 2.194 s,
records 156.32 ms); typed IR 19.218 s; LLVM lowering/checks 20.637 s; LLVM finish
0.772 s; native codegen 77.567 s; lookup/codegen/link 80.233 s. Direct-load lookup
median 2.874 s, Runtime admission 0.056 ms, useful execution 39.09 ms. Nested
phase values must not be added as independent costs. Peak RSS medians: cold
2,013 MiB; IR-retaining reuse 1,229 MiB; source-validated reuse 878 MiB.

Unmodified production compiler 21k whole-process wall: 19.800 s, versus instrumented
20.011 s median. One-helper edit: 19.601 s; imported managed-layout edit: 20.358 s.
Isolated 50/250-helper units took 0.897/4.671 s (LLVM codegen 0.721/3.706 s);
these demonstrate favorable finer-granularity potential, not a working incremental
cache with shared catalogs/imports. A five-launch small-program probe measured
7.37 ms spawn/loader/static initialization, 5.65 ms main-to-result and 15.58 ms
whole-process lifetime. No reboot or cold-storage startup is claimed.

LLVM Support SHA256 five-run medians: compiler 234.01 ms; 2.383 MB encoded source
records 9.32 ms; 82,648-byte object 0.322 ms; 10.283 MB object 40.64 ms; 51.531 MB
object 201.63 ms. The 5.694 s direct-load result excludes cache format/integrity and
owned-buffer filesystem overhead. Adding measured source/object hashing alone is
about 5.90 s, a projection, not a complete persistent-cache launch measurement.

Fresh-process normal/O2 objects executed combined Stage 4, interface witnesses,
module calls and a captured 2 MiB fixed array correctly. Same-object concurrent
JITDylibs, changed-body replacement (new 48/old lease 47), layout rejection, host
destruction with live code, Runtime budget-1 collection and final cleanup passed.
The large capture also passed at 1 MiB stack. Invalid current source failed before
loading; LLVM rejected a truncated object. Twelve concurrent LLVM hard-link
publishers produced one complete winner and eleven existing-entry refusals;
unlinking temporary names preserved the winner. No crash/power-loss guarantee is
inferred from that Linux primitive test.

### Exact commands and retained evidence

Commands ran from the repository root. Official research and hosted baseline reads:

```sh
git rev-parse HEAD
git status --short
gh run view 39174408997 --repo Shmellyorc/Void.Language --json headSha,jobs,conclusion,number > build/stage5-cache-research/ci68.json
gh api 'repos/llvm/llvm-project/contents/llvm/lib/ExecutionEngine/Orc/CompileUtils.cpp?ref=llvmorg-19.1.7' -H 'Accept: application/vnd.github.raw+json' > build/stage5-cache-research/CompileUtils-19.1.7.cpp
gh api 'repos/llvm/llvm-project/contents/llvm/lib/ExecutionEngine/Orc/LLJIT.cpp?ref=llvmorg-19.1.7' -H 'Accept: application/vnd.github.raw+json' > build/stage5-cache-research/LLJIT-19.1.7.cpp
gh api 'repos/llvm/llvm-project/contents/llvm/lib/Support/Windows/Path.inc?ref=llvmorg-19.1.7' -H 'Accept: application/vnd.github.raw+json' > build/stage5-cache-research/Windows-Path-19.1.7.inc
gh api 'repos/llvm/llvm-project/contents/llvm/lib/Support/Unix/Path.inc?ref=llvmorg-19.1.7' -H 'Accept: application/vnd.github.raw+json' > build/stage5-cache-research/Unix-Path-19.1.7.inc
```

Reproduction scripts contain exact compiler/link invocations, fixture generator,
phase probes and per-process resource collection. `build.py` compiles copied
compiler.cpp and the backend/driver with the flags above and links existing Release
core/runtime plus the exact LLVM libraries from CMake's voidc link command.
`link_small.py` reuses that research object (renaming its main with llvm-objcopy)
for the direct-source hit and lease probes. Main timing runs completed before the
remaining performance comparisons; no full compiler matrices ran during them.

```sh
python3 build/stage5-cache-research/prepare.py
python3 build/stage5-cache-research/generate.py
python3 build/stage5-cache-research/build.py
/usr/lib/llvm-19/bin/llvm-objcopy --redefine-sym main=benchmark_main build/stage5-cache-research/driver.o build/stage5-cache-research/lease_support.o
python3 build/stage5-cache-research/link_small.py fast
python3 build/stage5-cache-research/link_small.py lease
python3 build/stage5-cache-research/measure.py
bash build/stage5-cache-research/after_measure.sh
python3 build/stage5-cache-research/publication_probe.py
python3 build/stage5-cache-research/startup.py
python3 build/stage5-cache-research/report.py
```

`after_measure.sh` contains the individual object capture/load, edited-source,
isolated-unit, O2/interface/large/module, invalid-source/truncation, lease, hash and
small-stack commands. Small standalone SHA256/publication tools link installed
19.1.7 LLVMSupport/LLVMDemangle with Clang 19 and the matching LLD. Raw numeric
results: `measurements.json`, `fast_measurements.json`, `extra_measurements.json`,
`startup.json`, `llvm-hash-costs.txt`, `source-hash-cost.txt`, `summary.txt` and
per-launch `.txt` files. Generator/build/helper scripts are local disposable
research material, not a supported benchmark framework. The proposal retains the
fixture recipe, measured tables, methodology and limits independently of build files.

Focused existing production verification (no test/assertion changes):

```sh
cmake --build build/ci-release --target voidc void_tests void_advanced_tests void_gc_schemas -j2
ctest --test-dir build/ci-release -R '^(analysis|tooling|sessions|reload|gc_schemas|advanced_source|advanced_cli)$' --output-on-failure -j1
cmake --build build/ci-sanitizers --target void_tests void_gc_schemas -j2
ASAN_OPTIONS=detect_leaks=1:halt_on_error=1:exitcode=87 UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1:exitcode=86 ctest --test-dir build/ci-sanitizers -R '^(analysis|tooling|sessions|reload|gc_schemas)$' --output-on-failure -j1
git diff --check
```

**Results:** Release **7/7 passed, 12.79 s**; strict ASan/UBSan/leaks **5/5 passed,
2.60 s**. Expected negative probes rejected; all positive research runs passed.
Research execution/timing and LSan ran outside sandbox tracing. Initial scratch
harness errors (Expected reference unwrapping, missing Managed collection guard,
missing ArrayRef include and unavailable `/usr/bin/time`) were corrected in ignored
research only; they were not production defects. Python's existing process resource
API supplied per-process wall time and peak RSS. No production repair was indicated.

**Remaining approval:** the recommended whole-graph native-object architecture,
embedded build identity and implementation authorization. First-time edit latency,
actual reboot/crash recovery, cross-platform object/filesystem behavior and complete
cache end-to-end performance remain explicit implementation/acceptance work.
No commit, push, branch, remote mutation or hosted CI launch was performed.

## 18 October 2026: Stage 5 first production Debug native cache

**Authorization/baseline:** owner approved STAGE5_CACHE_PROPOSAL.md and all four
implementation milestones. HEAD remains 483e67e407b65674bdc82d44767f29bd83d73ae8;
Stage 4 remains hosted-locked at CI #68. This entry supersedes the preceding
architecture entry's pending-approval boundary. No commits, branches, pushes,
remote mutations, hosted workflow or Stage 6–7 work were performed.

### Production implementation

- Added optional `voidc run FILE INPUT --cache` and embedding `DebugCache` policy.
  Full current-source analysis/specialization and the shared entry rule precede
  hits. Misses use the existing typed IR and LLVM 19.1.7 lowerer. Release AOT and
  ordinary/O2 JIT paths remain available without persistence. Custom native
  bindings bypass persistence. Generic-template entry diagnostics remain identical
  to uncached execution; entry validation is shared rather than duplicated.
- CMake embeds a content identity from actual CLI objects, compiler/Runtime and
  LLVM archives plus configuration/toolchain settings. Unidentified shared LLVM
  compiler libraries disable persistence. Keys include target CPU/features/layout,
  exact reachable source/import graph and entry/compatibility/constant/Runtime mode.
  No Git/mtime identity, duplicate dependency graph or serialized frontend was added.
- Added a private descriptor-relative POSIX `.void/cache/v1` store with owned bytes,
  SHA-256 envelopes, private file/directory checks, exclusive temporary writes,
  fsync and non-replacing hard-link publication. Missing/unsafe/corrupt/failed
  artifacts fall back to normal compilation. Root and nested project cache directories
  are Git-ignored. Interrupted temporary files are ignored;
  corrupt final entries remain misses until disposable cache removal. Windows
  safely bypasses persistence pending verified ACL/path publication support.
- Source native generations have separate ORC JITDylibs/ResourceTrackers and retain
  existing NativeCode leases. Cold objects and hits admit relocated metadata through
  the existing Runtime ABI; redundant host metadata construction was removed.
  Raw-IR clients retain their established exported-symbol linking behavior.
- Compatible replacement uses the same cache path and existing Runtime identity.
  ToolingSession publication checks the current revision before compilation and
  again before publication. Incompatible changes still require restart. No
  automatic watcher, lazy JIT, native partitioner, GC or ownership registry was added.
- Added `debug_cache` and `debug_cache_processes`; existing 80 suites remain intact.
  Tests exercise fresh processes, source/import/generic/build invalidation, malformed
  native objects, symlinks/permissions/path failures, interrupted temporaries,
  concurrent first writers, collection/roots/publication, allocation rollback,
  callbacks/events, large arrays, optimized/custom-import bypass, current/stale
  revisions, incompatible layouts and old-generation leases.

### Demonstrated defects and corrections

1. Initial consolidated Debug and Release runs each passed 81/82: existing `jit`
   reported `Symbols not found: [ calculate__v1 ]`. Isolating every raw-IR module
   broke its explicit cross-module native-export contract. The unchanged regression
   holds a provider lease and imports its non-entry export from a second module.
   Source generations now isolate; trusted raw-IR compilation retains main-dylib
   linking and ResourceTracker retirement. Focused Debug `jit`, both cache suites,
   `reload` and `advanced_source` then passed 5/5 (43.40 s).
2. A real CLI child with `RLIMIT_FSIZE=(1,1)` returned -25 (SIGXFSZ) while optional
   cache publication wrote beyond the OS limit. The writer now checks the actual
   process file-size limit before creating/writing its envelope and skips persistence
   when it cannot fit. This introduces no array/object quota or signal-handler
   changes. A subprocess regression requires successful execution, no published
   artifact, then normal miss/hit after the limit is removed. Debug cache regressions
   passed 2/2 (19.56 s) after this repair.
3. Review found an allocation outside the writer’s noexcept boundary: constructing
   its NativeArtifact argument copied the captured object after successful native
   compilation. A linked fault-injection probe reproduced `std::bad_alloc` escaping
   that exact call shape (write-allocation-before.txt). The writer now accepts a
   non-allocating string_view and coordination flag; all envelope allocation remains
   inside its guarded fallback. A zero-budget publication regression covers this
   boundary. No compiler/Runtime allocation failure is suppressed.
4. Initial Make generation lacked a target ordering edge for CLI object hashing;
   explicit `void_cache_identity` -> `voidc_objects` ordering fixes clean object
   availability. Generation ownership also now starts before fallible JITDylib/
   tracker setup, so failed candidates retire correctly. The candidate NativeCode
   lease is prepared before Runtime metadata admission, preserving the original
   cold-path commit ordering even if lease allocation fails. Cache identity fields
   include module/import counts to make graph grouping unambiguous.

The first sanitizer build was launched under sandbox tracing and LSan correctly
refused to operate (`LeakSanitizer does not work under ptrace`). It was rerun
outside tracing with strict leak/error settings unchanged; no sanitizer was disabled.

### Reproduction and final verification commands

All build directories use LLVM 19.1.7, `VOID_LLVM_RESEARCH=OFF`:

```sh
cmake -S . -B build/order1/production
cmake -S . -B build/ci-release
cmake -S . -B build/ci-sanitizers
cmake --build build/order1/production -j2
cmake --build build/ci-release -j2
ASAN_OPTIONS=detect_leaks=1:halt_on_error=1:exitcode=87 \
UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1:exitcode=86 \
  cmake --build build/ci-sanitizers -j2
ctest --test-dir build/order1/production --output-on-failure -j2
ctest --test-dir build/ci-release --output-on-failure -j2
ASAN_OPTIONS=detect_leaks=1:halt_on_error=1:exitcode=87 \
UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1:exitcode=86 \
  ctest --test-dir build/ci-sanitizers --output-on-failure -j2
```

Builds independently regenerate/link the existing normal/O2 AOT executables. Final
configuration outcomes, small-stack and production measurements follow below.

| Final configuration | CTest result | Elapsed |
| --- | --- | --- |
| Linux Debug (`build/order1/production`) | 82/82 passed | 380.05 s |
| Linux Release (`build/ci-release`) | 82/82 passed | 158.04 s |
| Strict ASan/UBSan/leaks (`build/ci-sanitizers`, RelWithDebInfo) | 82/82 passed | 352.54 s |

These runs include the unchanged 80-suite inventory and both new cache suites,
normal/O2 JIT, independently linked AOT, GC/failure cleanup, collections,
results/patterns, closures/events, source/constant/layout reload and tooling tests.
The final cache policy/ownership, publication-allocation and warning assertions
were rebuilt and checked first using
`ctest --test-dir BUILD -R '^(jit|debug_cache|debug_cache_processes|reload)$' --output-on-failure -j1`
in each build (4/4 passed: Debug 20.16 s, Release 3.75 s, strict sanitizers 12.23 s).
Logs are in ignored `build/stage5-implementation/` (`*-delivery-ctest.log`,
`*-delivery-focused.log`); no failing assertion was weakened.

Additional verification:

```sh
python3 build/stage5-implementation/small-stack.py
PATH=/home/shmellyorc/.cache/codex-runtimes/codex-primary-runtime/dependencies/node/bin:/tmp/void-phase11-tools/node_modules/.bin:$PATH npm --prefix editors/vscode test
# In editors/vscode, using the same PATH:
node --test --test-isolation=none out/test/config.test.js
VOIDLS_PATH=/home/shmellyorc/Documents/Void.Language/build/order1/production/voidls \
VSCODE_EXECUTABLE_PATH=/usr/share/code/code npm run test:host
npm run package
```

The scratch small-stack runner uses Python `resource.setrlimit(RLIMIT_STACK,
(1048576,1048576))` in each native child and strict sanitizer settings. In **each**
configuration it runs arrays_foundation, the complete frontend suite, the existing
semantic_depth.py regression, the 2 MiB advanced/large source program through
normal/O2 JIT and linked normal/O2 AOT, then fresh-process cached cold/hit runs.
**27/27 checks passed**, all large-array results 17, including cached execution.
Commands/results are preserved in small-stack.json and small-stack.log.

Editor build/typecheck passed; direct Node test reporting confirms **4/4 cases**.
Installed real VS Code passed all three existing host profiles: trusted unsaved
repair/ranges, navigation/references, stale queries, lifecycle/restart/failure
cleanup; restricted trust (no native child); multi-root explanatory failure (no
native child). VSIX packaging passed: 149,208 bytes, SHA-256
`c012baf2dcc6730e81e5481adc8ec997ffa4102aee391d5d1ab6371327ab1269`.

A scratch invocation of the actual CacheBuildIdentity.cmake generator confirmed
unchanged object bytes retain the identity, changed object bytes and configuration
invalidate it, and unidentified shared compiler input disables persistence. Evidence:
build-identity-check.txt. No production object/archive was altered for this probe.

### Actual production performance and remaining boundary

```sh
python3 build/stage5-implementation/measure-production.py
python3 build/stage5-implementation/report-production.py
```

All **34 fresh-process production runs passed**, asserting stdout/results and cache
statuses. The 105,243-line / ten-module / 5,000-function workload measured:
uncached 142.332 s, cold cache miss 142.689 s, unchanged warm median **5.681 s**
(three launches: 5.519–6.606 s), one-helper edit 145.964 s, deliberate corruption
144.940 s. Cold/warm peak RSS: 2,020.6 / 940.0 MiB (warm median). One artifact:
51,141,049 bytes including its 129-byte envelope. Outputs were 60,000 before the
edit and 60,001 after. The full five-workload table, smaller generated projects,
real combined-language and 2 MiB payload cases are in STAGE5_CACHE_PROPOSAL.md;
raw production-measurements.json/.log includes every sample, CPU time and RSS.

These use the real Release-built production compiler with Debug/no-O2 guest code,
not the earlier prototype. They cover launch to successful process exit, including
execution/teardown; the native cache was cleared for cold samples, OS page caches
were not flushed. No compiler build/test ran concurrently. One cold/edit/corrupt
sample and three warm samples per generated workload are evidence, not a latency
SLA. The 2 MiB example remained dominated by execution (0.539 s cold, 0.493 s warm
median); no unrelated Runtime optimization is justified by that result.

**Delivery boundary:** all four authorized implementation milestones are complete
locally. All existing Orders 1–5 / Phases 1–25 / Stage 3–4 behavior and suites remain
intact; Stage 4 stays hosted-locked. Stage 5 is ready for owner-controlled hosted
CI, not hosted-verified. Windows persistence intentionally bypasses safely;
Windows fallback/CMake integration and macOS persistence still need hosted tests.
Builds using unidentified shared LLVM compiler libraries bypass persistence too.
No reboot/power-cut test, automatic watcher, function-granular cache, eviction
service or remote/shared cache is claimed. First-time edits recompile the graph;
corrupt exact keys remain misses until cache removal. Compiler/Runtime source,
cache tests, generated build-identity helper and updated status/contract documents
are present. No Git operation or hosted action was performed.


## 18 October 2026: CI #69 Windows source-generation native helper resolution

**Baseline:** `ebfdaf84fb17c9ea8901f4e7c8f0654017b44191`, initially clean.
Read-only inspection of [CI #69](https://github.com/Shmellyorc/Void.Language/actions/runs/39191134603)
and both completed Windows job logs confirmed successful builds followed by the
same 16/82 failed suites in Debug and Release. Source-generation materialization
reported missing `__chkstk`; both cache suites and raw-IR `jit` passed. The overall
run was still active when inspected, so individual completed job logs were fetched
through the read-only jobs API. No hosted run was launched or modified.

### Root cause and correction

Stage 4 (`483e67e407b65674bdc82d44767f29bd83d73ae8`) loaded source modules into
LLJIT's main JITDylib. LLVM 19.1.7 gives main direct default links to its platform
and process-symbol JITDylibs. Stage 5 created each isolated source generation with
`ExecutionSession::createJITDylib`, then linked only main. ORC symbol searches do
not recursively traverse main's own link order. Explicit `vd_*` native imports
remained available because `Jit::bind_address` defines them directly in main;
platform/process helpers were absent from the generation's search order.

The Windows x64 ABI requires the real stack probe for large fixed native frames.
LLVM can introduce that call during machine-code generation even when no source
or typed-IR import names it. The LLVM process lookup path includes existing
Windows explicit helper bindings; no new helper implementation is needed.
Smaller cache fixtures did not exercise that missing helper, and raw-IR modules
continued loading in main with its correct default links.

The repair appends `jit->defaultLinkOrder()` directly after main in each isolated
generation's link order. This uses LLVM 19.1.7's supported ORC APIs and preserves
lookup priority: generation, explicit main imports, then LLVM platform/process
symbols. No global process generator, native-binding registry or platform-specific
workaround was added. Source generations remain separate; disk-object hits and
cold compilation share this path. Raw-IR linking, metadata admission, precise GC,
ResourceTracker/NativeCode ownership and retirement are unchanged.

Exact-version sources inspected (retained under ignored `build/ci69-repair/`):

- [LLJIT.cpp](https://github.com/llvm/llvm-project/blob/llvmorg-19.1.7/llvm/lib/ExecutionEngine/Orc/LLJIT.cpp): process generator setup, default links, and LLJIT's `createJITDylib` wrapper.
- [LLJIT.h](https://github.com/llvm/llvm-project/blob/llvmorg-19.1.7/llvm/include/llvm/ExecutionEngine/Orc/LLJIT.h): supported `defaultLinkOrder` API, also checked against installed 19.1.7 headers.
- [ORC Core.cpp](https://github.com/llvm/llvm-project/blob/llvmorg-19.1.7/llvm/lib/ExecutionEngine/Orc/Core.cpp): lookup walks the supplied search order.
- [Windows DynamicLibrary.inc](https://github.com/llvm/llvm-project/blob/llvmorg-19.1.7/llvm/lib/Support/Windows/DynamicLibrary.inc) and [explicit_symbols.inc](https://github.com/llvm/llvm-project/blob/llvmorg-19.1.7/llvm/lib/Support/Windows/explicit_symbols.inc): LLVM's existing platform helper lookup.
- [Microsoft x64 prolog/epilog ABI](https://learn.microsoft.com/en-us/cpp/build/prolog-and-epilog): `__chkstk` probes large native allocations while preserving argument registers and RAX. It cannot be replaced by an ordinary C stub.

### Permanent regressions and causal evidence

The initial causal probe imported real native `abs` from a source-identified
isolated generation without `Jit::bind`. Before the backend repair, it failed
locally with `Symbols not found: [ abs ]` in `void.g2`, proving the same missing
process-link mechanism independently of Windows; after repair it passed. The
permanent `jit` regression uses real native `tolower` in normal and O2 modes. A
second check explicitly binds that name to the existing native fixture and verifies
that main's explicit binding wins over process symbols. Existing cross-module
raw-IR provider/consumer checks remain intact.

The new precedence assertion initially used `abs`; Debug focused testing caught
that its O2 form could not test binding precedence, because LLVM replaces `abs`
with `llvm.abs.i32`. A standalone exact-version `opt -passes='default<O2>'` probe
confirmed this transformation (`abs-call-o2.ll`). The regression now uses the
locale-dependent `tolower` call, which retains real native resolution in both
modes. No production optimization or assertion was disabled. Corrected Release
`jit` passed; corrected Debug focused verification passed 6/6, 28.52 s.

`debug_cache_processes` now generates 160 string locals, requiring a large precise
root frame, and executes fresh-process cache miss/hit (or the existing Windows
unavailable fallback), uncached normal JIT and O2 JIT. No artificial alloca,
stack-probe suppression, dummy helper or weakened assertion was added. A local
normal emitted object for this exact fixture reserves `0x3230` (12,848) native
stack bytes, independently confirming it exceeds a Windows stack page. This is
Linux frame/codegen evidence, not a claim of Windows execution. Existing cache
lease/coexistence, source revisions, GC and failure regressions are preserved.

### Local verification

All configurations use LLVM 19.1.7 with `VOID_LLVM_RESEARCH=OFF`. Initial focused
Release verification passed 6/6 (`jit`, `reload`, both cache suites,
`fixed_arrays_source`, `properties_source`), 10.71 s. Before-fix rejection is saved
in `before-jit.log`; build/test logs, upstream source snapshots, Windows job logs
and stack-frame disassembly are under ignored `build/ci69-repair/`.

Commands (repository root; existing build configurations retained):

```sh
cmake --build build/ci-release -j2
cmake --build build/order1/production -j2
ASAN_OPTIONS=detect_leaks=1:halt_on_error=1:exitcode=87 \
UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1:exitcode=86 \
  cmake --build build/ci-sanitizers -j2
# Focused checks in each configuration before consolidated acceptance:
ctest --test-dir BUILD -R '^(jit|reload|debug_cache|debug_cache_processes|fixed_arrays_source|properties_source)$' --output-on-failure -j2
ctest --test-dir build/ci-release --output-on-failure -j2
ctest --test-dir build/order1/production --output-on-failure -j2
ASAN_OPTIONS=detect_leaks=1:halt_on_error=1:exitcode=87 \
UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1:exitcode=86 \
  ctest --test-dir build/ci-sanitizers --output-on-failure -j2
python3 build/ci69-repair/small-stack.py
git diff --check
```

Sanitizer builds, focused/full tests and the small-stack runner execute outside
sandbox tracing with every strict sanitizer/leak option retained. The scratch
small-stack runner applies `RLIMIT_STACK=1 MiB` to each child and runs the exact
160-string fixture through normal/O2 uncached JIT and fresh-process cached
cold/hit paths in Debug, Release and strict sanitizers: **12/12 passed**.
`small-stack.json` retains commands, status, stdout and stderr. No editor or
language-server behavior changed; no unrelated editor/package work was added.

| Final local configuration | Complete CTest result | Elapsed |
| --- | --- | --- |
| Linux Debug (`build/order1/production`) | 82/82 passed | 356.27 s |
| Linux Release (`build/ci-release`) | 82/82 passed | 140.78 s |
| Strict ASan/UBSan/leaks (`build/ci-sanitizers`) | 82/82 passed | 329.61 s |

The final strict sanitizer focused run passed 6/6 (15.59 s). All three complete
builds succeeded and regenerated/linked the existing normal/O2 native AOT
executables. The unchanged 82-suite inventory includes both cache suites and
cross-module raw-IR linking. Final `git diff --check` passed. Build logs retain
existing toolchain/header and aggregate-initializer warnings; none was suppressed.

**Delivery boundary:** one production link-order correction plus permanent native
resolution/precedence and large-frame process regressions. All final local checks
passed. Windows and macOS execution of the repair remains unverified locally and
requires owner-controlled hosted acceptance; Stage 5 is not declared hosted-locked.
No stack probes, cache/isolation behavior, GC checks or existing assertions were
disabled. No commit, push, branch, remote mutation or hosted CI action was performed.

## 18 October 2026: Stage 5 hosted lock and Stage 6 architecture investigation

**Authorization:** research/proposal and documentation reconciliation only. No
Stage 6 compiler, Runtime, library, scheduler or test implementation was authorized
or performed. Local HEAD was clean at
`4627687b3a18349633715e4294bc915cf74ddd41` before documentation edits.

### Verified hosted baseline

The owner declared Stage 5 locked at CI #70. Read-only Actions metadata identifies
[run 39193640484](https://github.com/Shmellyorc/Void.Language/actions/runs/39193640484),
number 70, at that exact HEAD, conclusion success. All seven completed job logs
independently contain their successful 82/82 inventory: Windows Debug/Release,
macOS ARM64 Debug/Release and Ubuntu Debug/Release/strict sanitizers. **574/574
hosted suites passed.** Assertions over metadata and every per-job CTest summary
are retained in `build/stage6-investigation/ci70-summary.json`, with raw metadata
and downloaded logs beside it. The aggregate `gh run view --log` returned empty
output; per-job read-only API downloads supplied the actual evidence instead.

The canonical strategy, current known issues, Bible status, Wrench handoff and
Stage 5 proposal now reflect that lock. ADVANCED_LANGUAGE.md clarifies that the
cache was delivered separately. Stage 4 retains its own historical CI #68 lock.
Stage 5 acceptance covers its delivered subset: Windows persistent disk caching
still bypasses safely; macOS cache suites and Windows fallback passed hosted verification.
Automatic watching, finer native partitions and power-loss durability remain
unimplemented/unverified as previously documented. Earlier dated entries remain
history. The Stage 5 guide also now describes the CI #70 direct platform/process
link order, rather than the superseded main-only generation link.

### Investigation and recommendation

Created [STAGE6_PROPOSAL.md](STAGE6_PROPOSAL.md). Inspected the current Language
Bible concurrency/native/reload contracts and unresolved decisions; development
strategy; complete GC_RUNTIME and ADVANCED_LANGUAGE guides; known issues; actual
runtime coordination, handles/frames, callback admission/close, checked native ABI,
JIT/AOT lowering, source cleanup/capture lowering, IR types/CFG, reload slots and
native provider fixtures. Source evidence includes:

- `include/void/runtime.hpp`, `src/runtime/coordination.cpp`: TLS attachment,
  Managed/NativeSafe/Parked rendezvous, precise collection and Handle ownership.
- `include/void/ir.hpp`, `src/frontend/compiler.cpp`: synchronous typed CFG,
  concrete callables/captures and dynamic LIFO cleanup, with no task/suspension IR.
- `src/backend/llvm/backend.cpp`: direct field loads/stores, root frames,
  NativeSafe imports, generation isolation/default links, cache and revision checks.
- `include/void/execution.hpp`, `src/runtime/reload.cpp`: synchronous generation
  leases, exact compatibility and generation-borrowed diagnostics.
- `include/void/callback.hpp`, `src/runtime/callback.cpp`, runtime callback and
  coordination tests: provider quiescence, foreign/same-thread reentry and shutdown.
- `src/runtime/abi.cpp`, `src/runtime/native.cpp`, native C fixtures: current
  internal C ABI/string/resource boundaries, not an implemented source FFI.

The principal finding is that existing thread coordination makes collection safe;
it does not define concurrent source mutation or freeze readonly aliases. A pool
alone cannot safely enable arbitrary managed-reference or closure spawn. A second
finding is that suspended task leases are necessary but insufficient for callable
values escaping a completed task; their generation lifetime also needs a policy.
Neither gap is represented as an existing compiler defect.

Recommendation: explicit stackless lowering in the existing typed IR, ordinary
traced heap frames, persistent active-task roots and existing NativeCode leases;
one owner executor for async source; a bounded pool with a proposed first
noncapturing/value-only source spawn boundary and transitive effect checks.
Cancellation and groups share checked cleanup, with suspension-based joins and
explicit draining shutdown. Source native libraries form a separate bounded
compiled-bridge/wrapper delivery. No duplicate backend, collector, task VM,
work-stealing system, generic FFI engine or package service is proposed.

The proposal contains five implementation deliveries, regression requirements,
risks and six owner decision groups: parallel sharing/effects, call/handle semantics,
entry/scope ownership, cancellation/group cleanup, escaping-callable leases and
native binding scope/representation. Every new behavior and restriction is labeled
as recommended and unapproved. No example or foreign-language behavior was promoted
to VOID's contract. The owner must approve semantics and authorize a selected
implementation sprint before Stage 6 production work starts.

### Upstream research and checks

Official LLVM 19 coroutine documentation and exact `llvmorg-19.1.7`
`llvm/docs/Coroutines.rst` were inspected, along with primary documentation for
libuv work/completion scheduling, Python task-group failure/cancellation, Rust
Send/Sync, C++ data races and Microsoft's DLL loader/lock rules. Direct source
links and the relevance/limits of each comparison are in STAGE6_PROPOSAL.md.
The exact LLVM document is retained in ignored `build/stage6-investigation/`.
No experimental Stage 6 algorithm or timing was claimed or added to production.

Commands ran from the repository root:

```sh
git rev-parse HEAD
git status --short
gh run list --repo Shmellyorc/Void.Language --limit 8 --json databaseId,number,headSha,conclusion,status
gh run view 39193640484 --repo Shmellyorc/Void.Language --json headSha,jobs,conclusion,number
# For each of the seven returned databaseId values:
gh api repos/Shmellyorc/Void.Language/actions/jobs/JOB_ID/logs
gh api 'repos/llvm/llvm-project/contents/llvm/docs/Coroutines.rst?ref=llvmorg-19.1.7' -H 'Accept: application/vnd.github.raw+json'
ctest --test-dir build/ci-release -R '^(runtime_coordination|runtime_callbacks|runtime_generated|jit|reload|debug_cache|debug_cache_processes|native)$' --output-on-failure -j2
git diff --check
```

Existing focused Release baseline checks passed **8/8, 5.66 s**. Their log is
`build/stage6-investigation/baseline-focused.log`. This rechecks mechanisms used
by the recommendation; it does not validate proposed async/scheduler behavior.
No rebuild or full local matrix was needed for documentation-only changes.
Markdown/local-link and documentation-scope validation are recorded with the
research evidence. No commits, pushes, branches, remote writes or hosted CI
operations were performed. Owner approval/implementation remains outstanding.


## 18 October 2026: Stage 6 Delivery 1 authorization and implementation start

Owner approved Native Async Activation only. Before production edits, persisted
the exact Stage 5 lock, five sequential deliveries, D1–D6 approval/restriction
mapping, owner-controlled hosted gates, risks and pending root-adapter lifecycle
question in WRENCH_CLANKER.md, STAGE6_PROPOSAL.md and canonical development status.
Existing uncommitted architecture/status documents are preserved. Implementation
and acceptance are pending; no later delivery or Git/CI operation is authorized.

Owner clarified the root lifecycle: drain all started/unawaited tasks before
return, preserve root failure as primary, report otherwise unobserved failures
when root succeeds, deterministic order and complete roots/leases/cleanup.
No implicit cancellation, detached tasks or public groups. Recorded in the
proposal, Wrench handoff and canonical status before implementation proceeds.

Delivery 1 is in progress, not accepted. Added source/IR async effects, inferred
task shapes, native heap frames/start/resume lowering and an explicit root API;
the first normal/O2 activation/await source fixture passed. Expanded tests found
and are repairing no-result await statement recognition, snapshot-helper source
ownership and fixed-result rooted handoff. Broader regression, allocation, GC,
AOT/cache/reload and full local acceptance are pending. Native ready wakeup uses
nonallocating task links. No later delivery is active.

Delivery 1 implementation research/repair evidence (in progress):
- Native start/resume lowering uses existing typed CFG, failure/cleanup and root
  machinery; snapshot helpers are ordinary validated IR rather than a second
  array-copy ABI. Task/frame allocation is rooted before publication; enqueue and
  wakeup need no allocation. Scope ownership rejects competing/nested executors.
- Owner approved ordinary async functions and managed receivers, with value
  receivers, virtual/interface methods and async function values diagnosed until
  their calling/lifetime contracts are reviewed. Persisted the decision and
  rationale in the proposal and Wrench handoff; these features remain deferred.
- Expanded normal/O2 JIT plus five normal AOT focused suites passed 7/7 (58.39 s).
  The initial fixed-result failure was an unregistered task-payload copy source;
  loading into the existing registered instruction home before copying corrected
  it. An initial failure fixture used a compile-time-invalid negative u8 literal;
  replaced it with an input-dependent checked conversion, preserving the failure.
- Added managed receivers, result/value/generic forwarding and readonly profiles.
  Investigation found two integration defects: inferring async-call effects from
  the returned task-shaped type misclassified synchronous generic forwarding;
  calls now record their actual declaration effect. Task payload authority also
  needs to survive await. Existing logical reference paths now carry immutable
  task result profiles/provenance, while physical LLVM root maps trace only the
  task pointer. Treating the task's aggregate payload as a pointer map initially
  failed descriptor admission; the pointer special case now requires managed_ref.
- Added root-success/root-failure unobserved sibling draining and reached-defers
  observations, concurrent-root rejection, GC overlap with a suspended parent,
  live old-generation replacement/retirement, nested-root and foreign-generation
  publication rejection. Source activation tests passed with these cases before
  the final trace and malformed-IR additions. Await now shares synchronous call
  trace propagation rather than discarding the caller location.
- Malformed task-shape coverage exposed an out-of-range field lookup before
  validation. Task shape is now independently validated before reading its result
  authority; added forged construction and live borrowed-place negatives.
- Strict sanitizer build under the traced sandbox hit LeakSanitizer's documented
  ptrace fatal error. Re-running the unchanged strict leak/ASan/UBSan settings
  outside tracing; no sanitizer suppression or test weakening was introduced.

Current verification is not final acceptance. Full local matrix runs are in
progress and must be rebuilt after the last verifier/trace changes. Delivery 1
is not hosted-accepted, and no later delivery is authorized.

A focused borrowed-receiver probe (`build/stage6-investigation/borrow.log`)
confirmed that `view.method(await task)` retained a prepared interface place.
The IR verifier rejected it, as the proposal requires, but source checking had
accepted it. Added a source-located diagnostic with the safe explicit-local
rewrite, plus a source negative. Ordinary constructor arguments evaluate before
staging and do not acquire this restriction. This keeps the existing suspension
boundary rather than inventing a prepared-place lifetime contract.
The provisional full Release matrix passed 97/98; its old async_ir executable
caught the malformed-task out-of-range lookup described above. The corrected
independent verifier and added forged-construction/borrowed-place negatives pass
in focused Debug. Final rebuilt acceptance remains pending. Retained `spawn` as
an ordinary identifier in this delivery; no spawn grammar was introduced.
The provisional full Debug matrix passed 97/98 and caught the original 1 MiB
semantic-depth regression: placing await's Type temporaries in the recursive
expression dispatcher enlarged every synchronous recursive dispatch frame. Moved
await checking into the existing non-inlined semantic-handler pattern; kept the
original depth limits and small-stack test unchanged. Added managed string
argument/result suspension to the GC/allocation fixture. Final rebuilt matrix
will verify these corrections with no lowered guards or sanitizer suppressions.
Independent async IR execution now covers normal/O2 directly, rather than only
source-generated IR. Its required task descriptor must be in the generation's
nominal catalog; added a missing-catalog negative to prevent valid-looking IR
from failing descriptor admission only at execution. Tightened the private task
shape against unrelated enum/collection flags and recorded async effects in raw
IR reload contracts. Explicitly diagnose async deinit, preserving the existing
synchronous lifecycle/cleanup contract. Final source cases now include successful
unawaited descendants created after root completion and sibling failure creation
order even when the first-created failure completes last.
A focused generic-constructor probe reached the internal missing-struct assertion
for an inferred task type. Added a source diagnostic at `t()` and a negative,
preserving the approved no-public-task-constructor boundary. The independent IR
constructor rejection remains in place. No language-facing task constructor or
new task API was added.

## 19 October 2026: Stage 6 Delivery 1 local completion — STOP for owner review

Implemented Native Async Activation only: `async func`, inferred internal tasks,
`await`, native start/resume state machines, precisely traced zero-initialized heap
frames, one owning executor thread and an explicit synchronous-ABI root adapter.
Argument order, fixed value copies, managed/nullable/string identities, readonly
profiles, generic forwarding, synchronous local closures and reached cleanup are
covered. Managed receiver identity survives variable rebinding and suspension.
Unsupported async lifecycle/value-receiver/virtual/interface/callable forms and
prepared interface receivers across await diagnose before LLVM lowering. Task
payload access/construction is private; generic task construction diagnoses too.

The root adapter drains all started work, including descendants created after
root completion. Root failure remains primary; otherwise unobserved failures are
reported in creation order, even when completion order differs. Checked await
failure shares existing caller-trace and cleanup propagation. No cancellation,
detachment, public groups, parallel spawn, general native libraries or watching
was introduced. `spawn` remains an ordinary identifier in this delivery.

Existing NativeCode leases enclose complete draining and every resumption; no task
can be resumed outside its owning root scope. Generation identity rejects foreign
publication. Live replacement changes private frame layout and suspension-state
count while the old root remains suspended; its original state/code resumes,
then the old generation retires. Task/frame trace maps and diagnostic strings are
owned independently of retired code. Persistent Debug source-cache cold/hit paths
and all original raw-IR linking/native-helper regressions remain covered.

The final tree was frozen after the corrections documented above. Earlier partial
and mixed-build logs are investigation evidence, not acceptance; superseded local
verification processes were stopped before the final frozen build. The final
checks below all passed without weakened tests, guards or sanitizer settings.

| Final local configuration | Result | Time | Evidence under `build/stage6-investigation/` |
| --- | --- | --- | --- |
| Clang/LLVM 19.1.7 Debug, research OFF | 98/98 | 421.90 s | `delivery1-debug-acceptance.log` |
| Clang/LLVM 19.1.7 Release | 98/98 | 156.70 s | `delivery1-release-acceptance.log` |
| RelWithDebInfo, strict ASan/UBSan/leak checks | 98/98 | 349.47 s | `delivery1-sanitizers-acceptance.log` |

**294/294 full-suite passes**, preserving all original 82 suites and adding 16:
async source, independent async IR and seven native AOT fixtures in normal/O2.
Source coverage includes GC root inspection/forced collection, exhaustive native
allocation budgets through first success in both modes, deterministic failure
and cleanup draining, cache, active reload/retirement, competing/nested executors,
foreign generation rejection and unsupported-source negatives. Independent IR
executes normal/O2 and rejects malformed effects/task shapes/catalogs, uninitialized
required locals, forged task access/construction, cleanup awaits, invalid control
flow and borrowed local/instruction values surviving suspension.

Additional focused evidence: final source/IR **2/2, 42.29 s** in
`delivery1-frozen-focused.log`; unchanged 1 MiB semantic-depth/recovery checks in
`delivery1-frozen-depth.log`; 2 MiB fixed-array async AOT normal/O2 both returned
27 on **1 MiB native stacks**, recorded in `delivery1-small-stack.json`.
Documentation local-link validation passed (`delivery1-doc-links.json`), and
`git diff --check` passed. Structured totals are in `delivery1-acceptance.json`.

Final commands from the repository root (build output retained in matching logs):

```sh
cmake --build build/order1/production -j4
ctest --test-dir build/order1/production --output-on-failure -j2
cmake --build build/ci-release -j4
ctest --test-dir build/ci-release --output-on-failure -j2
ASAN_OPTIONS=detect_leaks=1:halt_on_error=1:exitcode=87 UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1:exitcode=86 cmake --build build/ci-sanitizers -j3
ASAN_OPTIONS=detect_leaks=1:halt_on_error=1:exitcode=87 UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1:exitcode=86 ctest --test-dir build/ci-sanitizers --output-on-failure -j2
python3 tests/integration/semantic_depth.py build/order1/production/voidc
git diff --check
```

Strict sanitizer build/tests ran outside tracing because LeakSanitizer cannot run
under ptrace. Leak detection and halt/error exit settings stayed enabled. The
normal/O2 small-stack probe uses Python `resource.setrlimit(RLIMIT_STACK,
(1048576, 1048576))` and executes both `void_async_aot_large_*` binaries with input7.
No editor/package change was made or claimed.

Deliverables: production compiler/IR/backend/runtime changes, seven fixtures in
`examples/async`, regressions in `tests/async`, [ASYNC.md](ASYNC.md), updated
proposal/Wrench/canonical handoff and GC/language/known-boundary guides. Existing
uncommitted Stage 6 research and Stage 5 reconciliation were preserved.

**Remaining:** owner review/commit/push and the seven-job hosted run. Windows and
macOS were not executed locally. Stage 5 stays LOCKED at
`4627687b3a18349633715e4294bc915cf74ddd41`, CI #70, 7/7 jobs and 574/574 suites.
Delivery 1 is locally complete, not hosted-locked. Root-scope task/frame retention
is conservative until drain; very long roots can grow with total created work.
Other calling contracts, cancellation/groups, parallelism and native libraries
remain the approved later-delivery review work. No Delivery 1 owner decision or
failing local check remains. No commits, pushes, branches, remote writes or hosted
CI actions were performed. **STOPPED. Do not begin Delivery 2 without hosted
acceptance and explicit owner authorization.**

## 19 October 2026: Windows CI #71 missing iterator header repair

Owner-reported Windows Debug and Release compilation failures at
`8257237d3b2d984cb0b52537f90e20a52ef1e959` have the same cause:
`src/ir/async_validation.inc:16` calls `std::inserter`, but its including
translation unit did not explicitly include `<iterator>`. Clang-cl 19.1.7 with
MSVC STL reports `no member named 'inserter' in namespace 'std'`. The local Linux
build's transitive includes had masked this missing standard-header dependency.

Added only `#include <iterator>` alongside the standard-library includes in
`src/ir/module.cpp`. No standard header was added to the function-included
fragment. IR validation logic, tests, sanitizer settings and constraints are
unchanged.

Focused Linux Debug verification passed:

```sh
cmake --build build/order1/production --target void_async_tests -j4
ctest --test-dir build/order1/production -R '^async_ir$' --output-on-failure
cmake --build build/order1/production --target void_tests -j4
ctest --test-dir build/order1/production -R '^(core|async_ir)$' --output-on-failure
git diff --check
```

Both build targets succeeded. The standalone async IR check passed 1/1; the final
core/async IR regression run passed **2/2, 0.08 s**. Logs are
`/tmp/void-ci71-focused-build.log`, `/tmp/void-ci71-focused-ir.log`,
`/tmp/void-ci71-core-build.log` and `/tmp/void-ci71-ir-regressions.log`.
No additional failure appeared, so no broader investigation or changes were made.
Windows compilation is pending owner push and cross-platform verification; no
local Windows success or Delivery 1 hosted lock is claimed. No commits, pushes,
branches or CI actions were performed. STOPPED for owner verification.

## 19 October 2026: Windows Debug CI #72 editor keyword-test repair

Owner-confirmed CI #72 at `ee42c39c8ffa50096c87b6e4cdbf0d15e5fd1f19` passed
Windows Release. Windows Debug built the compiler and passed all 98/98 native
CTest suites; its remaining failure was the VS Code extension unit assertion
`Unexpected keyword highlighting: async`. The test still classified `async` as
unsupported after the approved grammar change made it a control keyword.

Updated only `editors/vscode/src/test/config.test.ts`: added `async` to the
positive keyword expectations, removed it from the negative expectations, and
added an assertion that one `keyword.control.void` rule matches both `async` and
`return`. All other positive/negative assertions, the actual grammar and native
implementation remain unchanged.

Using the extension's required Node 24.19.0 runtime, both requested commands
passed from `editors/vscode`:

```sh
PATH=/home/shmellyorc/.nvm/versions/node/v24.19.0/bin:$PATH npm test
PATH=/home/shmellyorc/.nvm/versions/node/v24.19.0/bin:$PATH npm run package
```

`npm test` completed type checking, compilation and the configuration test file
with zero failures. Packaging succeeded and produced
`editors/vscode/dist/void-language-0.1.0.vsix` (8 files, 145.71 KB).
Logs: `/tmp/void-ci72-editor-test.log` and `/tmp/void-ci72-editor-package.log`.
`git diff --check` passed. No native rebuild or unrelated changes were needed.
No commits, pushes, branches or CI actions were performed. STOPPED for owner
push and hosted verification; no new hosted acceptance or lock is claimed.


## 19 October 2026 — Delivery 2 authorized

Owner confirms Delivery 1 hosted acceptance at `34efb761965a5e19841e2fcf2816a5b851e902a7`, CI #73: seven green jobs, 98/98 each (686/686). This is owner-provided evidence. Delivery 2 assignment approves its eight lifecycle rules; canonical handoffs now preserve the authorization and Delivery 3–5 gates. Research: existing executor owns Task records/precise handles until root drain; checked failure edges enter reached LIFO cleanup, but there is no per-activation child/group drain or cancellation state. Implementation and acceptance pending. No Git or remote modifications authorized.


## 19 October 2026 — Delivery 2 implementation and focused evidence

Implemented in the existing owner-thread executor and LLVM state-machine lowering: native success/cancelled/failed outcomes; idempotent task cancellation; starts/await/CFG checkpoints; implicit activation ownership and contextual lexical task groups; shielded asynchronous joins; early-exit cancellation; failure wakeup/sibling cancellation; children-before-parent cleanup. Ordinary async resource `with` scopes own child work until join/deinit. Cleanup cannot indirectly publish fresh async work. Cancellation is separate from RuntimeFailure while defers run, so actual cleanup failures supersede cancellation. Existing parent runtime failure and additional defer diagnostics survive; unobserved children across lexical groups are selected in creation order. Existing typed frame roots, Runtime ownership and invocation generation leases/cache remain in use.

Independent IR validation checks cancellation operands, opaque group homes/uses, LIFO joins/CFG merges, required drain, and illegal bypass/repeated drain/work after cleanup. Source rejects first-class group escapes, unsupported spawn/group arguments, cancellation arguments and deferred await. Cancellation requests remain nonsuspending and can be forwarded through inferred synchronous helpers/cleanup. No worker pool, parallel spawn, native library or public cancellation recovery was added.

Focused Debug CTest (`async_source`, `async_ir`, `async_lifecycle`) passed 3/3 in 50.14s during development (`/tmp/void-d2-focused.log`). Subsequent audit strengthened failure aggregation across groups and retention of additional defer failures; final consolidated acceptance covers those corrections. Real fixtures cover normal/early/Err exits, before-start/suspended cancellation, repeated/completed awaits, implicit/nested scopes, cleanup failure ordering and parent precedence. Allocation sweeps, cache/reload, semaphore-gated cancellation/GC/retirement, close/drain and private-ABI self/ancestor waits extend those fixtures. D1 failure tests now assert D2 sibling cancellation and no unreached defers, rather than expecting cancelled bodies to run. No tests/sanitizers were disabled. Full Debug/Release/strict acceptance pending below.


Final audit reproduced a contextual-name collision: a visible global `const tasks = factory()` with an ordinary `group()` resource method was incorrectly treated as the builtin task namespace. `/tmp/void-d2-bound-tasks-before.log` records the pre-fix diagnostic. Recognition now uses the checker's existing complete visible-name set (plus lexical lookup), preserving constants/types/functions/imports and local bindings. A permanent normal/O2 source regression covers the factory resource method. Group-alias capture also preserves its restricted marker through existing closure lookup, preventing a first-class escape before lowering. Cancellation requests support inferred synchronous helpers and nonsuspending cleanup, while enqueue from cleanup remains rejected. Full frozen-source matrices are repeated after these audit corrections; earlier diagnostic builds are not final acceptance evidence.


Acceptance hardening: AOT expected-failure tests now require the checked CLI status **1**, so strict sanitizer exit codes 86/87 or crashes cannot masquerade as expected task failure. All six failure cases passed in each of Debug, Release and strict sanitizers (18/18 focused checks). A targeted array/value-struct return probe confirmed the existing owned-field snapshot path already preserves the value evaluated before group draining even when a child's cleanup mutates the managed source (expected/actual 17); no additional copy mechanism was added. The real `lifecycle_values.void` fixture preserves this behavior in normal/O2 JIT/AOT and allocation/GC checks. Final suite count is 113.


## 19 October 2026 — Delivery 2 final local acceptance / STOP for owner review

Delivery 2 is complete locally. No incomplete implementation, failing local test
or unresolved Delivery 2 approval question remains. Locked baseline remains
`34efb761965a5e19841e2fcf2816a5b851e902a7` (owner-confirmed CI #73, 7/7,
98/98 each / 686/686); HEAD remains unchanged. Delivery 2 has **no hosted lock**.

| Final frozen-source configuration | Result | CTest elapsed |
| --- | --- | --- |
| Debug — `build/order1/production` | **113/113 passed** | 339.73 sec |
| Release — `build/ci-release` | **113/113 passed** | 156.43 sec |
| Strict ASan/UBSan/leaks — `build/ci-sanitizers` | **113/113 passed** | 319.20 sec |

**339/339 total.** Compiler SDK verified locally as LLVM **19.1.7**. Each tree
completed `cmake --build <tree> -j 3` followed by
`ctest --test-dir <tree> -j 3 --output-on-failure`. Strict build and test both used
`ASAN_OPTIONS=detect_leaks=1:halt_on_error=1:exitcode=87` and
`UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1:exitcode=86`, outside tracing so
LeakSanitizer can operate. No sanitizers, checks or existing suites were disabled.
Earlier diagnostic/intermediate runs are superseded by these complete matrices.

Additional final evidence: frontend recursive receiver/type grammar guards passed
at **1 MiB**; independently linked normal/O2 async `large` and
`lifecycle_resources` binaries passed at 1 MiB in all three configurations
(**12/12**, including strict sanitizers). Expected-failure AOT checks additionally
passed **18/18** with exact checked exit status 1, preserving sanitizer/crash
failures. VS Code `npm test` passed (one reported test file); `npm run package`
passed, producing the existing 8-file / 145.71 KB VSIX locally. No editor sources
or dependency versions changed in this delivery.

Permanent new source fixtures: `lifecycle`, `lifecycle_exits`,
`lifecycle_cancelled`, `lifecycle_failures`, `lifecycle_primary`,
`lifecycle_resources`, `lifecycle_values`, each normal/O2 JIT and independently
linked AOT. The lifecycle suite adds forced precise GC, allocation sweeps, cached
group activation, compatible reload/old-code retirement under cancellation,
semaphore-gated GC rendezvous, Runtime close/drain, namespace binding, cleanup
publication rejection and compiler negatives. Independent IR cases cover valid
synchronous cancellation forwarding, required drain, malformed cancellation,
forged/escaping groups, LIFO ownership, bypassed/duplicate joins/drains and prior
initialization/borrow/suspension proofs. Native helper tests reject self/ancestor
wait cycles and duplicate publication. Cancellation-before-start runs no
unreached defer; reached child cleanup finishes before parent cleanup/deinit;
parent failure and additional cleanup failures survive deterministically.

Deliverables: source/backend/runtime and permanent tests in the working tree;
updated ASYNC, proposal, Wrench handoff, canonical strategy, Bible/status,
GC runtime and known issues. Final build/CTest/stack/editor transcripts and
focused failure-status/reproduction evidence are retained in
`build/stage6-delivery2/` (ignored local artifacts).

**Next action belongs to Boss/Wrench:** review the uncommitted Delivery 2 diff,
then owner commit/push and the existing seven-job hosted gate. Lock Delivery 2
only after green hosted acceptance. Deliveries 3–5 remain pending and unauthorized;
parallel spawn/workers, native libraries, detached tasks, cancellation recovery,
general await in cleanup and automatic watching were not added. No commits,
pushes, branches, remote modifications or hosted CI actions were performed.


## 19 October 2026 — Delivery 3 baseline and authorization

HEAD verified clean at `5d228804be809465ce2c97b7e0b58b89caa718ac`. Owner confirms
Delivery 2 hosted acceptance, CI #74, 7/7 jobs, 113/113 each (791/791).
Delivery 3 authorizes initial value-only bounded parallel spawn; Deliveries 4–5
remain pending. Canonical handoffs preserve this superseding status before implementation.
Research and implementation pending; no Git/remote modifications authorized.


## 19 October 2026 — Delivery 3 implementation and focused research

Existing async Task records, root invocation leases, precise Handle/frame maps,
Runtime admission and the shared LLVM 19.1.7 lowerer supply the implementation.
Source spawn creates a private typed-IR worker wrapper around a directly resolved
synchronous function. Existing capability traversal records its resolved call /
reference graph and uses inferred reference-effect summaries; no second effect
lattice or compiler is introduced. Independent IR checks enforce safe recursive
transfer types and reject task/indirect/native effects in worker graphs.

The scheduler now owns a lazy bounded native pool (default 4; idle host config
1–64). FIFO linked admission reuses Task records; storage remains memory bounded
by actual allocation failure, with no source task-count ceiling. Worker-private
heap frames/results remain precisely rooted. Release/acquire completion hands
publication back to the owning executor; no parent activation is written by a
worker. Managed admission and user code run outside queue locks; idle and owner
notification waits are NativeSafe. Once-admitted workers drain through Runtime
close. Explicit synchronous CFG checkpoints implement cancellation separately
from GC polling and preserve shielded reached cleanup / cleanup-failure precedence.
Owner checkpoints receive completions so child failure interrupts a busy owner.

Focused tests use actual generated source loops and condition-variable rendezvous
on two distinct worker threads (no sleep/timing proof), pool-one native FIFO and
foreign-generation rejection, forced worker GC, worker/caller allocation failure,
compatible reload leases, close/drain, running cancellation, sibling failures and
cleanup-failure precedence. JIT normal/O2 and permanent linked AOT fixtures cover
fixed arrays, strings, result payloads, argument order, no-result/repeated await and
groups. LSP overlay tests cover spawn definition/unsafe effects/recovery; grammar
checks cover spawn/await as ordinary control keywords. Final acceptance pending.

Development evidence: the first focused run caught a fixture naming collision
(`var counter = counter()`), corrected to `owner`; compiler shadowing checks were
preserved. A later five-suite focused run passed async source/IR/lifecycle/parallel
and LSP, 5/5 in 47.13s. Subsequent audit reuses the existing capability traversal
for call edges (including defaults, properties and resource cleanup), and avoids
mutating analyzed AST call nodes while emitting worker wrappers. These changes
require the final frozen-source checks below. No hosted/Git actions performed.


Final audit before acceptance tightened worker cancellation aggregation: only the
actual cancellation primary is replaced by a real cleanup failure. An inner
cleanup's own primary/additional failures remain intact until merged at that
boundary; no string scanning infers outcomes. The deterministic running-cancel
test now includes nested failing defers and verifies both cleanup diagnostics
survive, with cancellation absent as primary. Focused async_parallel passed in
3.89s after this correction. Final matrices rebuild the audited sources; initial
build transcripts are preparation evidence only. Code is frozen for acceptance.


## Delivery 3 acceptance in progress — frozen production sources

Debug full CTest passed **120/120** in **321.02s**. Release full CTest passed
**120/120** in **138.30s**. A late fixture-only change adds a nested lexical group
to `spawn.void`; its Debug AOT objects were refreshed and both affected AOT suites
passed again (2/2, 0.09s). The Debug JIT suite and Release matrix also consumed that
final fixture. This correction changes no production code or test expectation.
Strict sanitizer acceptance is still pending below.

Additional checks passed: editor `npm test` and `npm run package` (8-file VSIX,
145.72 KB); existing frontend depth guards at 1 MiB for Debug/Release/strict;
normal/O2 independently linked `large`, `lifecycle_resources` and `spawn` binaries
at 1 MiB for all three configurations (18/18 native stack checks). LLVM SDK reports
19.1.7. Strict commands use real leak detection / failure exits, outside tracing:
`ASAN_OPTIONS=detect_leaks=1:halt_on_error=1:exitcode=87` and
`UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1:exitcode=86`.

Evidence is retained under `build/stage6-delivery3/` (ignored local artifacts).
The `*-final-build.log` files are final audited builds; first-pass `*-build.log`
files are preparation transcripts. No sanitizers, assertions or baseline suites
were disabled. No commits, pushes, branches or hosted actions were performed.


## 19 October 2026 — Delivery 3 final local acceptance / STOP

Delivery 3 is complete locally. No incomplete implementation, failing local test
or unresolved Delivery 3 language decision remains. The hosted-locked baseline
is still Delivery 2 at `5d228804be809465ce2c97b7e0b58b89caa718ac`, owner-confirmed
[CI #74 checks](https://github.com/Shmellyorc/Void.Language/commit/5d228804be809465ce2c97b7e0b58b89caa718ac/checks),
7/7 jobs, 113/113 each (791/791). HEAD is unchanged; Delivery 3 has no hosted lock.

| Final audited configuration | Result | CTest elapsed |
| --- | --- | --- |
| Debug — `build/order1/production` | **120/120 passed** | 321.02s |
| Release — `build/ci-release` | **120/120 passed** | 138.30s |
| Strict ASan/UBSan/leaks — `build/ci-sanitizers` | **120/120 passed** | 251.77s |

**360/360 total**, LLVM **19.1.7**. Final builds used `cmake --build <tree> -j 3`,
followed by `ctest --test-dir <tree> -j 3 --output-on-failure`. Strict build/tests
ran outside tracing with the exact ASan/UBSan/leak options recorded above. All
baseline suites remain enabled. The two affected Debug AOT suites additionally
passed after refreshing the nested-group fixture; production code was unchanged.

All three frontend 1 MiB depth checks and **18/18** native 1 MiB checks passed:
normal/O2 linked `large`, `lifecycle_resources`, and `spawn` in every configuration.
VS Code `npm test` passed; `npm run package` produced the existing 8-file VSIX
(145.72 KB). No dependency changes or sanitizer suppressions were introduced.

Permanent regressions include the `async_parallel` suite and source fixtures
`spawn`, `spawn_cancel`, `spawn_failure`, each normal/O2 independently linked AOT.
They cover real generated-source overlap via synchronization, worker bounds one
and multiple, FIFO admission, argument order, fixed copies, immutable strings,
private managed allocation, tagged results, no-result/repeated await and nested
groups. Additional coverage verifies worker/caller allocation failure, precise GC
rendezvous, running cancellation, nested cleanup failures superseding cancellation,
sibling failure/shielded drain, a busy owning activation observing completion,
close/shutdown, old-generation retirement under reload, actual Debug cache hits,
malformed worker IR, unsafe transfer/effects and foreign-generation publication.
LSP overlay definition/unsafe-effect/recovery and grammar keyword tests passed.

Deliverables are the production/frontend/IR/shared LLVM/runtime changes, permanent
regressions, and updated ASYNC, STAGE6_PROPOSAL, WRENCH_CLANKER, canonical strategy,
Bible, GC runtime and known-issue documents. Evidence is under `build/stage6-delivery3/`:
`*-final-build.log`, `*-ctest.log`, stack/editor logs, and the explicit Debug spawn
refresh/rerun transcripts. Historical preparation/focused entries remain intact.

Known bounds are documented rather than hidden: value-only transfers, no worker
child tasks/indirect/native effects, default four workers with idle host config
1–64, FIFO admission without multiworker start/finish fairness, memory-dependent
queue admission and root-duration completed-frame retention. General shared-memory
concurrency, native libraries, `jobs.spawn`, cancellation recovery and detach are
not delivered. These restrictions match the approved initial scope.

**Next action belongs to Boss/Wrench:** review Delivery 3; owner commit/push and
seven-job hosted CI; lock only when green. Deliveries 4–5 remain unauthorized.
No commits, pushes, branches, remote modifications or hosted actions occurred.


## 19 October 2026 — Delivery 4 baseline / bounded native authorization

HEAD verified clean at `62b17c8cdff80a2cf1afc08ecfc0ecf33c76a090`. Owner confirms
Delivery 3 CI #75, seven green jobs, 120/120 each (840/840). Delivery 4 authorizes
one statically linked trusted C fixture with typed project-local descriptors and
normal source wrappers; no public native grammar or general FFI. Research reuses
CallbackRegistration's provider barrier, precise Handle roots, NativeSafe boundaries
and generation leases. Runtime-native resource ownership must remain separate from
GC cleanup. Callback leases must not create a Runtime → generation → compiler →
Runtime ownership cycle. Implementation and acceptance pending; no Git/remote writes.

## 19 October 2026 — Delivery 4 implementation and focused evidence

Implemented an explicitly authorized project descriptor for the real statically
compiled `src/native/fixture.c`, ordinary `examples/native/*.void` wrappers and
one existing typed-IR/LLVM JIT/AOT path. The host supplies the verified virtual
binding header; no CLI/LSP auto-admission, public native grammar, symbol discovery,
dynamic loading or worker/native relaxation was added. Descriptor signatures,
compiled symbol identity and affinity/blocking/callback metadata fail closed.

The C++ compiled bridge applies the actual target C ABI for exact-width integer,
float and bool prototypes. LLVM's private transport carries only scalar bits and
pointers. UTF-8 terminated temporaries reject NUL; length-aware/returned/retained
bytes preserve it. Native storage is represented by nonforgeable managed identities
in a Runtime registry; native pointers never enter source fields. Existing result,
one-shot deinit and with release deterministically; GC never runs a native finalizer.

Native callback registrations reuse CallbackRegistration, real C register/unregister,
provider reservation/quiescence, captured precise roots and an exact generation
lease. Initial callback registration requires direct named/literal/bound creation;
forwarded callable values diagnose because existing callable discriminators have
no generation token. Resource calls are owner-thread only; blocking calls and
foreign dispatch/join are NativeSafe. Shutdown drains even reserved pre-admission
callbacks and serializes concurrent native shutdown calls. Registry/provider locks
are released before source execution, root release and native-code retirement.

Two lifetime hazards identified and corrected: retaining a generation through the
compiler Impl would form Runtime → callback → generation → Impl → Runtime, so
generations now retain the existing shared ORC engine/counter directly. Mutable
thread-local failure scratch could be overwritten/freed by a failing defer while
its primary error was still borrowed; native failures now have invocation-owned,
bounded diagnostics, copied before a callback lease ends and exported for AOT.
A permanent native primary-plus-cleanup failure program verifies preservation.

Focused Debug evidence: `native_source` and main/cleanup-failure normal/O2 AOT
passed 5/5 (`/tmp/d4-focus4.log`). Expanded `native_source` then passed in 9.36s
(`/tmp/d4-nativefinal.log`), including real C callback snapshots, cache cold/hit,
callback self-drain and foreign affinity rejection. Other permanent cases cover
all scalar widths, UTF-8/NUL, acquisition failure, retained/returned ownership,
checked close failures, allocation sweeps, foreign callback allocation/GC,
pre-admission provider teardown, forged source/IR, descriptor ABI/symbol mismatch,
lease retention/reload and shutdown. Consolidated acceptance is running; results
will be recorded below. Historical entries remain unchanged.

Final source was frozen after adding serialized native shutdown, explicit standard
header coverage and independent value-owner/task-transfer/worker-native rejection
cases. Preparation transcripts are retained as `*-prepare-build.log` and
`*-prepare-ctest.log`; each configuration refreshes the native Runtime object and
runs the complete matrix on that frozen source. Final Debug passed **131/131**
(301.16s); final Release passed **131/131** (129.35s), including the new source/IR
negatives. Both passed their 1 MiB semantic checks, eight normal/O2 native AOT
stack checks and explicit native-host execution (`19`). Editor `npm test` passed
(1/1); `npm run package` produced the VSIX. Strict final refresh/acceptance remains
in progress; it uses ASan/leak exit 87 and UBSan exit 86 with halt-on-error, outside
tracing. No sanitizer or existing assertion was disabled.


## 19 October 2026 — Delivery 4 complete local acceptance / owner handoff

The frozen implementation passed the consolidated final matrix:

| Configuration | Suites | CTest elapsed |
| --- | --- | --- |
| Debug | 131/131 | 301.16 sec |
| Release | 131/131 | 129.35 sec |
| Strict | 131/131 | 268.49 sec |

**393/393 suites passed**, preserving every original 120-suite configuration.
Strict acceptance retained ASan, UBSan (including host-generated callback function
metadata), leak checking and checked failure exits; both build and execution ran
outside tracing. The earlier preparation transcripts are superseded by these
refreshed final builds and tests, including the final native Runtime object.

Each configuration also passed the existing 1 MiB semantic-depth checks and eight
1 MiB independent native AOT runs: async large/lifecycle_resources/spawn and the
new native main, normal/O2 (**24/24 total**). All three explicit native example
hosts printed `19`. Editor `npm test` passed (1/1); `npm run package` produced
`editors/vscode/dist/void-language-0.1.0.vsix`. `git diff --check` passed.
No unresolved local test/sanitizer failure remains. Cross-platform hosted results
are pending; no Windows/macOS success is inferred from these Linux results.

Deliverables: the compiled C fixture/bridge and typed project descriptor, existing
frontend/IR/LLVM lowering and Runtime ownership changes, real ordinary VOID
wrappers/programs, permanent positive/negative source/IR/JIT/AOT regressions and
updated NATIVE_LIBRARIES/ASYNC/STAGE6_PROPOSAL/WRENCH_CLANKER/canonical strategy,
Bible, GC runtime and known-issue documentation. Evidence is retained under
`build/stage6-delivery4/`: final `*-build.log`, `*-ctest.log`, semantic/AOT stack,
host, editor/package and focused native transcripts; preparation logs remain too.

Known boundaries remain explicit: one trusted static fixture, host opt-in instead
of automatic CLI/LSP admission, direct/literal/bound generation-local callback
creation, one-shot registration, owner-thread resource calls, no resource task
transfer or worker/native effects. General native syntax/loading/packages and
broader callable/library ownership require future review. GC does not free native
resources; omitted deterministic release retains them until Runtime shutdown.

**STOPPED. Next action: Boss/Wrench review; owner commit/push and seven-job hosted
CI; lock Delivery 4 only when green.** Hosted baseline remains Delivery 3 at
`62b17c8cdff80a2cf1afc08ecfc0ecf33c76a090`, owner-confirmed CI #75 (840/840).
Delivery 5 was not begun and requires separate owner authorization after that
hosted gate. No commits, pushes, branches, remote modifications or hosted actions
were performed.


## 19 October 2026 — Delivery 5 baseline and integration authorization

HEAD verified at `619331ccb83227419081bc93cf47bd3a0b8b64cc`. Owner confirms
Delivery 4 hosted acceptance, CI #76, seven green jobs, 131/131 each (917/917).
Five uncommitted Stage 7 planning/workflow documentation changes predate this
sprint and must be preserved. Delivery 5 adds meaningful combined source/host
regressions and fixes only reproduced defects, preserving native owner affinity,
worker effect/transfer restrictions, one compiler/Runtime and owner-only Git/CI.
Focused integration checks precede consolidated acceptance. Stage 7 remains
unauthorized. Implementation/evidence will be recorded below.


## 19 October 2026 — Delivery 5 focused integration investigation

Added real source applications combining owning-executor async activation, lexical
groups, value-only workers and trusted C resources/retained buffers/same-foreign
callbacks. Shared emission selects the existing explicit async AOT adapter; no new
compiler, syntax, binding authorization or native worker effect is introduced.
The existing native harness now covers native-owner cleanup after cancellation,
parent native failure plus nested cleanup failure, GC in workers/callbacks,
whole-invocation allocation failure and provider shutdown while a worker and a
pre-admission foreign dispatch are held by deterministic barriers.

Reload tests hold one running worker and another published/queued job beneath
suspended async parents. A warm cached old generation keeps its frame/code and
native callback after replacement; the callback still computes its old result
once the root has drained and NativeCode is dropped. Unregister releases the old
lease. Four repeated compatible replacements per mode collect to zero managed
objects and one live current generation. At the held old-root barrier the observed
heap contains 14 objects; this is a precise object count, not RSS or a promise that
Runtime metadata/LLVM memory is immediately reclaimed. Broader root-duration
record retention remains unchanged. A retained old C callback attempting async
enqueue into a new generation's active root is checked and rejected.

Two initial test assumptions were corrected without changing production policy:
replacing cleanup/call-reference contracts can require restart, and an invalid
cache entry remains a miss until explicitly removed (publication never overwrites
it). Regression assertions now protect both existing contracts. A native cache
case previously used a nonexistent project root and therefore tested safe
unavailability; creating that root now exercises actual cold/hit reuse on supported
platforms. New fresh-process combined cache tests include a real stack-probed
worker frame and cold/hit/corrupt fallback, with the Windows safe bypass retained.
No demonstrated compiler/Runtime defect has required a production change.

Compiler snapshot tests and real LSP process tests verify async/spawn/group source
references, explicit native authorization rejection, overlay repair and stale
result/version rejection. Stage 7 features were not added. Focused Debug LSP,
native and integrated/cross-process suites passed 4/4 in 27.11s before the final
AOT-focused check. One-time setup failures (unbuilt new targets, a test-local name
collision and incorrect expected rejection text) were repaired in the harness;
none was treated as a production regression or hidden by weakening a safety check.
Complete consolidated acceptance is next; final results follow below.

The first consolidated Release pass exposed a race in the new cancellation
fixture, not in cancellation semantics: its seven-iteration worker could finish
successfully before `cancel()` reached it. Release normal AOT and all focused
checks had passed; optimized Release AOT correctly returned that completed result
(`7`) instead of cancelling an already-completed task. The regression now uses a
nonterminating cooperative worker with bounded integer state, following the
existing `spawn_cancel.void` pattern, so success cannot race the cancellation
request. The separate deterministic C-boundary regression still proves cancellation
of a worker held inside its real loop. No scheduler behavior, timing assertion,
safety check or expected cancellation diagnostic was weakened. Final acceptance
below supersedes that initial fixture failure; its log is retained.

## 19 October 2026 — Delivery 5 locally accepted; owner/hosted handoff

**STOPPED after complete local acceptance.** HEAD remains
`619331ccb83227419081bc93cf47bd3a0b8b64cc`; Delivery 4 remains the locked baseline,
owner-confirmed [CI #76](https://github.com/Shmellyorc/Void.Language/actions/runs/39359190869).
No commits, pushes, branches, hosted actions or remote changes were performed.
Stage 6 is **not yet finally locked**: Boss reviews/commits/pushes and runs the
seven-job hosted acceptance. Stage 7 remains unauthorized; its preexisting
editor/workflow planning changes are preserved. Wrench reviews architecture,
Keyboard delivers this local integration, Boss owns product/Git/CI decisions.

All 131 baseline suites remain; eight additions bring each matrix to **139**:
`stage6_integrated`, `stage6_cache_processes` and three new native source fixtures
in normal/O2 independently linked AOT. No compiler, Runtime, GC, LLVM lowering,
cache policy, editor production code or CI configuration changed. The harness
repairs are explicit async AOT adapter selection, real native-cache project-root
creation and the deterministic cancellation fixture described above.

| Final local evidence | Result | Log under `build/stage6-delivery5/` |
| --- | --- | --- |
| Debug full CTest | 139/139, 335.23s | `debug-ctest.log` |
| Release full CTest after fixture correction | 139/139, 121.69s | `release-final-ctest.log` |
| Strict ASan/UBSan/leak full CTest | 139/139, 313.99s | `strict-ctest.log` |
| Corrected fixture + combined host focused checks | 3/3 per configuration; Debug 12.08s, Release 6.47s, strict 12.86s | `*-correction-focused.log` |
| Semantic 1 MiB depth/recovery | Passed in all three configurations | `*-semantic-stack.log` |
| Independent normal/O2 AOT at actual 1 MiB | 14/configuration, **42/42** | `debug-final-aot-stack.log`, `release-aot-stack.log`, `strict-final-aot-stack.log` |
| Editor unit tests | 1/1 | `editor-test.log` |
| Real VS Code integration | Trusted, restricted-trust and multi-root profiles passed | `editor-host.log` |
| VSIX packaging | Passed, 145.72 KB | `editor-package.log` |

Full matrices total **417/417**. Debug/strict full matrices passed before the
cancellation-fixture correction; both were rebuilt and their affected host/AOT
checks rerun, then all 14 native stack runs rerun. The initial Release full pass
was 138/139; the initial strict 1 MiB optimized cancellation run also observed the
same legitimate completed-worker race. These superseded results remain in
`release-ctest.log` and `strict-aot-stack.log`. Release was rerun in full after the
correction. No unrelated full matrix or editor acceptance was repeated. The
pre-matrix focused LSP/native/combined/cache/AOT check passed **10/10 in 25.76s**
(`focused-ctest.log`). No local test failure remains.

Commands used (`B` is `build/order1/production`, `build/ci-release` or
`build/ci-sanitizers`; LLVM SDK independently reports **19.1.7**):

```sh
cmake --build "$B" -j3
ctest --test-dir "$B" -j3 --output-on-failure
ctest --test-dir "$B" -R '^(stage6_integrated|native_aot_integrated_cancel_(normal|optimized))$' --output-on-failure
python3 tests/integration/semantic_depth.py "$PWD/$B/voidc"
```

Every strict build/test/native stack command retained
`ASAN_OPTIONS=detect_leaks=1:halt_on_error=1:exitcode=87` and
`UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1:exitcode=86`, executed outside
tracing for LeakSanitizer. No sanitizer disabling/suppression or expected-failure
sanitizer exit was admitted. Real native stack checks used this temporary driver,
invoked as `python3 /tmp/d5_stack.py "$B"` (with those same strict variables):

```python
import pathlib, resource, subprocess, sys
root = pathlib.Path(sys.argv[1]).resolve()
cases = [('async','large','27'), ('async','lifecycle_resources','30'),
         ('async','spawn','126'), ('native','main','19'),
         ('native','integrated','32'),
         ('native','integrated_cancel','failure:Async task cancelled'),
         ('native','integrated_failure','failure:native text cannot contain NUL')]
for family, fixture, expected in cases:
    for mode in ('normal', 'optimized'):
        program = root / f'void_{family}_aot_{fixture}_{mode}'
        r = subprocess.run([str(program), '7'], capture_output=True, text=True,
            timeout=90, preexec_fn=lambda: resource.setrlimit(
                resource.RLIMIT_STACK, (1048576, 1048576)))
        if expected.startswith('failure:'):
            assert r.returncode == 1 and expected[8:] in r.stderr + r.stdout, r
        else:
            assert r.returncode == 0 and r.stdout.strip() == expected, r
```

Editor commands used Node 24.19.0 from the owner's available nvm installation:

```sh
npm --prefix editors/vscode test
npm --prefix editors/vscode run package
VOIDLS_PATH="$PWD/build/order1/production/voidls" VSCODE_EXECUTABLE_PATH=/usr/share/code/code npm --prefix editors/vscode run test:host
```

The real installed VS Code ran using the available display and isolated profiles;
trusted diagnostics/unsaved repair/definition/references/stale queries/restart/
configuration/failure/deactivation passed, while untrusted and multi-root profiles
spawned no native child. Package deliverable:
`editors/vscode/dist/void-language-0.1.0.vsix`. Existing package-size warning is
informational. No Stage 7 implementation or broad editor feature claim is implied.

Observed combined lifetime evidence in all three configurations, normal/O2:
14 managed objects at the held old-root barrier; two live generations during
replacement and after the old root/NativeCode drains while its registration stays
live; one generation after explicit unregister/close; four subsequent compatible
replacements per mode return to zero managed objects and one current generation.
Normal mode reports an actual cache hit; O2 intentionally bypasses persistence.
The running-worker/pre-admission shutdown case reaches zero participant/code
counts after provider and worker drains. Source/harness assertions, not timing
windows, verify those boundaries. These are object/generation observations, not
RSS measurements, metadata reclamation guarantees or a new retention policy.

Remaining boundaries: resources stay on the owning executor; worker/native effects,
resource transfer and new cross-task callable escape remain rejected. General FFI,
dynamic loading, implicit native authorization, automatic watching, async CLI and
shared mutable worker graphs remain outside this delivery. Windows stack-helper
resolution has a permanent combined fresh-process regression, but actual Windows
and macOS execution, packaging and all seven hosted jobs still require owner CI.
No local production defect or unresolved implementation failure was demonstrated.
Next action: owner review, commit/push, hosted verification; lock Stage 6 only if
all seven jobs pass. Do not begin Stage 7 without separate authorization.

## 19 October 2026 — CI #77 Windows integrated fixture byte-offset repair

Baseline verified locally: `d3bf74430f06cc094eda5dad18d65d7d7e9b0c2c`.
Owner reports [CI #77](https://github.com/Shmellyorc/Void.Language/actions/runs/39372676122)
Windows Debug/Release passed 138/139, with only `stage6_integrated` failing at
`spawn source reference missing`. Existing workflow documentation changes and
untracked `docs/RESEARCH_CLANKER.md` were preserved.

Verified the exact mismatch in the fixture: `write()` opened `std::ofstream` in
text mode, while compiler `read_source()` explicitly reads binary bytes. The
original LF `plain` string locates the spawn call's `work(value)` at byte **111**;
Windows LF-to-CRLF translation adds three preceding CR bytes and moves that same
reference to byte **114**. At byte 111 the CRLF source contains `wn work(val...`,
not the start of the callable identifier. The permanent regression explicitly
writes those CRLF bytes on every platform: real disk analysis rejects the original
LF query offset and resolves byte 114 to the same `work` declaration. This
reproduces the reported failure mechanism locally without claiming a local
Windows run or changing source-position policy.

Smallest fixture correction: open the existing integrated source writer with
`std::ios::binary`. LF and explicit CRLF fixtures now assert exact disk-byte
round trips and exact analyzed snapshot contents. The original definition,
historical snapshot, authorization, diagnostic repair and stale-result assertions
remain intact. The new CRLF positive lookup uses its own actual byte offset;
UTF-8/UTF-16 and CRLF production semantics remain unchanged. No compiler, Runtime,
LLVM, scheduler, GC, language behavior, expected outcome or test skip changed.

Focused local commands, for `B=build/order1/production` (Debug) and
`B=build/ci-release` (Release):

```sh
cmake --build "$B" --target void_native_tests -j3
ctest --test-dir "$B" -R '^(stage6_integrated|tooling|tooling_demo|lsp_positions|lsp_protocol|lsp_demo)$' --output-on-failure
```

Both builds passed. **6/6 each, 12/12 total**: Debug 11.38s, Release 5.86s.
This includes the integrated normal/O2 execution and new LF/CRLF regression,
compiler tooling/session tests, LSP UTF-8/UTF-16/CRLF coordinate tests, real LSP
protocol tests and tooling/LSP demos. Logs:
`build/stage6-delivery5/ci77-{debug,release}-{build,focused}.log`.
`git diff --check` passed. No additional full matrix was warranted by this
fixture-only correction. Windows hosted confirmation remains pending owner CI.

**STOPPED for owner review.** No commits, pushes, branches or remote actions.
Stage 6 remains pending final hosted acceptance; Stage 7 remains unauthorized.

## 20 October 2026 — Stage 7 research and roadmap

Boss reports Stage 6 locked at `4139297f92c195782f59a28546444e6b3bec4ad8`,
CI #78, seven jobs and 973/973. Local HEAD independently matches that commit;
research did not rerun or independently certify hosted CI. This owner-authorized
mission was research/planning only, with bounded technical experiments.

Deliverable: [STAGE7_ROADMAP.md](STAGE7_ROADMAP.md), with verified implemented
versus missing capabilities, compiler/session reuse, incomplete-source completion,
rename/formatter safety, project/refresh/lifecycle policy, official client/version
sources, platform packaging, one cohesive sprint sequence, focused tests, acceptance
and decisions for Wrench/Boss. Permanent raw measurements and reproducible probes:
[research/stage7/README.md](research/stage7/README.md).

Rebuilt Release voidls/voidc and passed 6/6 focused tooling/session/LSP/Stage 6
integration suites (6.54s). Extension typecheck/build/unit runner, all three existing
real VS Code 1.141.0 profiles and VSIX packaging passed. Real Neovim 0.12.6 and
Vim 9.2 + yegappan/lsp c741b2f baseline attachment/unsaved-error/repair/navigation/
stop probes passed using temporary fixtures and isolated runtime paths. No future
completion/rename/formatter behavior is claimed by these baseline probes.

Seven-sample Linux Release measurements include real analysis/generic/advanced/
async fixtures and generated 3/17/33-module projects. Large graph valid edit median
233.93ms, queued 20-edit burst 4800.57ms with 660 diagnostic notifications; this
supports batching before speculative incremental compilation. Real editor invalid
edit/repair medians 304.85/304.33ms; definition command median 1.28ms (70 samples).
The pinned language client has a 250ms pending-change delay; causation was not fully
profiled. Native dependency inspection shows the local executable still needs
system libraries; a standalone portable bundle is not yet verified.

Important constraints: compiler-owned scopes/recovery are missing; new tooling
records must not accidentally alter native compatibility serialization. Current
Vim client lacks declared documentChanges support and a TextDocumentEdit version
check; do not advertise safe rename to it. Boss decisions include entry/rename
scope, formatting style, Vim parity, distribution/license and supported matrix.
VSCodium, minimum editor releases, new-client Windows/macOS execution and clean
native distribution remain unverified. Full production regression/sanitizer runs
belong to the authorized implementation sprint, not this documentation work.

Initial experiment fixture/setup issues were corrected in the research scripts;
no production source/test assertions were weakened. No production code, workflow,
LLVM version, Git history or hosted state changed. **Research complete; STOPPED for
Wrench review and Boss authorization.**


## 20 October 2026 — Stage 7 engineering sprint, local acceptance complete

Boss authorized one cohesive production sprint after the Stage 6 lock. Implemented
compiler-owned editor queries/bindings, completion on bounded incomplete contexts,
hover, signature help, symbols, conservative validated/versioned rename, compiler
lexer formatting with semantic/idempotence checks, save/watch/explicit refresh,
bounded ordered transport batching/cancellation, parent-process cleanup and richer
VS Code discovery/trust/entry/lifecycle. Added shipped built-in Neovim and pinned
Vim LSP adapters with syntax/filetype/indent and stale-edit guards. Vim rename stays
disabled. Added native install/package tooling, private Linux archive/platform VSIX,
real editor/install tests and permanent setup/contracts/evidence. LLVM/Clang/LLD
stays 19.1.7; new tooling records do not alter native contract serialization.

The 1 MiB gate exposed a new cold-probe temporary on recursive frames; isolated
that construction. Fresh HEAD also reproduced pre-existing capability analysis
(96 calls) and lowering (64 calls) stack crashes. Narrow noinline capability/IR
case boundaries now preserve evaluation order and accept legal 16/64/96/124 chains
on 1 MiB; excessive chains retain normal diagnostics. Depth guards and native
assertions were not weakened. Four representative freshly built HEAD/current
compatibility/constant contracts remain byte-identical. An argument-expression
signature-selection bug was repaired using AST membership rather than comma offsets.
Initial URI/fixture/installed-host harness issues were corrected without removing
safety assertions. No known failing final local acceptance test remains.

Final frozen-source builds: `cmake --build <dir> -j2` and
`ctest --test-dir <dir> --output-on-failure -j2`:

- Debug `build/order1/production`: **140/140**, 0 failed, 427.92 s.
- Release `build/ci-release`: **140/140**, 0 failed, 165.36 s.
- RelWithDebInfo `build/ci-sanitizers`, strict ASan/UBSan/leaks: **140/140**,
  0 failed, 365.19 s. `ASAN_OPTIONS=detect_leaks=1:halt_on_error=1:exitcode=87`,
  `UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1:exitcode=86`; executed outside
  tracing sandbox because LSan cannot run under ptrace.
- Separate `ulimit -s 1024` frontend CTest gates passed in Debug (10.60 s) and
  Release (4.55 s); strengthened semantic-depth integration passed in all matrices.
- Extension typecheck/build/unit tests passed. Real VS Code **1.141.0** development
  extension and actual installed final Linux VSIX each passed trusted, restricted
  trust and multi-root profiles (3/3 each), including feature/refresh/restart cleanup.
- Installed **NVIM v0.13.0-dev-1410+g510f61555b**, Release/LuaJIT
  2.1.1787165859, passed shipped-adapter headless completion selection, incomplete
  signatures, hover, symbols, definitions, diagnostics/repair, guarded cross-file
  rename, stale-version/format rejection, formatting/idempotence, refresh,
  restart/stop and no orphan checks against the final Release server.
- Portable Neovim **0.12.6** passed the same adapter suite against Debug.
  Actual **Vim 9.2 patches 1–858**, `/usr/bin/vim.basic`, with yegappan/lsp
  `c741b2f1afb42b9f267824dac4049674908f4a41` passed completion/signatures,
  hover/symbols/navigation, rename-disabled, dirty repair, guarded formatting,
  refresh/restart/stop/no-orphan checks. The installed `vim` alternative resolves
  to Neovim; selected actual Vim explicitly, preserving all user configurations.
- Native archive extraction/handshake with SDK PATH removed, CMake component
  installation/real LSP demo and actual bundled-server VSIX installation passed.
  Final Release/archive/CMake-installed/VSIX native bytes have the same SHA-256.

Seven-sample same-host Release measurements: large 33-module queued 20-edit burst
**4800.57 → 277.77 ms**, diagnostics **660 → 33** (about 17.3× shorter).
Single valid-edit median **233.93 → 271.76 ms** (sample p95 292.14 ms); idle RSS
**94,596 → 129,584 KiB**. Metadata/revalidation costs are explicit, not an overall
compiler-speedup claim. Large completion/signature/hover/rename/format medians:
**191.79 / 186.72 / 0.11 / 430.02 / 253.96 ms**; request timings exclude prior
synchronization and edits are not applied. Completion workload uses incomplete
member access; existing call-callee replacement may conservatively return none.
Seven-observation p95 is only the largest measured sample. Exact commands, raw
before/after/example/query measurements and profiles are in
[stage7/ACCEPTANCE.md](stage7/ACCEPTANCE.md) and adjacent JSON files. Copied final
logs are under ignored `build/stage7-acceptance/`.

Deliverables: `build/stage7-artifacts/void-tooling-linux-x64.tar.gz` and
`void-language-linux-x64.vsix`, with LLVM notices/dependency audits/private-acceptance
warnings. Same-host launch does not verify clean-machine portability. VSCodium,
Windows/macOS and research minimum editor versions remain unverified; extension
minimum now matches tested VS Code ^1.141.0. Recovery/rename/formatting exclusions
are explicit in STAGE7_IMPLEMENTATION.md and KNOWN_ISSUES.md. Public redistribution
requires Boss's licensing/dependency/release decision; repository remains UNLICENSED.
No unresolved architectural decision blocks the implemented subset.

No commit, push, branch/remote mutation, publication or hosted CI action occurred.
Original HEAD remains `4139297f92c195782f59a28546444e6b3bec4ad8`.
**STOPPED for Wrench review and Boss-controlled commit/push/seven-job hosted CI.
Stage 7 is not locked.**

## 20 October 2026 — CI #79 Windows Stage 7 fixture encoding repair

Windows Release passed 139/140 suites; `lsp_stage7` failed before starting its
client because default CP1252 text output cannot encode the Unicode fixture.
Read-only review of completed Windows Debug job 118629041717 confirms the same
`stage7.py:19` UnicodeEncodeError and 139/140 result; no additional failed suite.
Changed all five fixture `write_text` calls in `tests/lsp/stage7.py` to explicit
UTF-8 byte writes, preserving LF bytes/offsets on Windows. The corresponding text
read now specifies UTF-8; the existing byte restoration and Unicode coverage stay.
No compiler/LSP production code changed.

Focused checks passed: `ctest --test-dir build/ci-release --output-on-failure -R
'^lsp_stage7$'` (1/1, 0.14 s), the same selection in `build/order1/production`
(1/1, 0.19 s), and the complete Stage 7 script with default-encoding warnings
and Python UTF-8 mode disabled. An instrumented full-script run rejected implicit
text writes/reads and checked all six byte writes (including restore) for UTF-8
round-trip and LF-only bytes. `git diff --check` passed. These are Linux checks;
the repair awaits Boss commit/push and Windows hosted revalidation. No Git
mutation or hosted dispatch occurred; unrelated website work was preserved.


## 20 October 2026 — CI #80 rename URI portability repair

Read-only CI #80 logs confirm the exact mapping assertion failure on Windows
Release and macOS Release (139/140); Boss also confirmed Windows Debug and the
same macOS failure in CI #79. Added ASCII-safe actual/expected URI/version mapping
diagnostics before changing production behavior. Reproduced locally using a
symlinked temporary root: actual paths contained `/tmp/void-ci80-real/`, expected
paths `/tmp/void-ci80-alias/`, while both mappings carried main version **11** and
closed-worker **null**. An additional pre-fix regression demonstrated URI
re-encoding (uppercase escapes versus the client's lowercase escapes/localhost
spelling), again with correct version **0** and closed-worker null. The defect
is outbound filesystem/URI canonicalization, not lost versions or a missing rename
edit. Original hosted logs lack mappings, so their exact path spelling differences
cannot be recovered retroactively; the new assertion exposes them on any recurrence.

The server now retains the exact URI supplied at didOpen and uses it for that
open document's edits/locations/diagnostics. Closed files use the original selected
entry-root path, while canonical compiler/VFS identity and rename revalidation stay
unchanged. Nonempty diagnostic publication also retains its URI so close/reopen
under another spelling clears the old diagnostics. Rename rechecks the selected
root's canonical identity and rejects an alias retargeted during server lifetime.
No weakening of capability checks, signed versions, binding equivalence, closed-file
byte checks, or exact URI assertions. No compiler/language semantics changed.

Expanded `lsp_stage7` checks exact URI/version mappings **and both complete edit
sets/ranges/replacement text**. Coverage includes Unicode/reserved path bytes,
lowercase percent escapes, localhost/scheme spelling, Windows drive-letter spelling,
zero/positive/negative versions, changes through canonical URI aliases, reopen,
closed-file null versions, stale-diagnostic cleanup, POSIX aliased roots and alias
retarget rejection. The previous executable fails the added URI regression;
the repaired executable passes it without normalizing the expected edit mapping.

Focused final commands: `cmake --build <dir> --target voidls void_lsp_tests -j2`,
then `ctest --test-dir <dir> --output-on-failure -R '^lsp_'`. Debug
`build/order1/production`: **4/4**, 0.81 s; Release `build/ci-release`: **4/4**,
0.49 s; strict ASan/UBSan/leaks `build/ci-sanitizers`: **4/4**, 1.75 s
(`ASAN_OPTIONS=detect_leaks=1:halt_on_error=1:exitcode=87`,
`UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1:exitcode=86`, outside ptrace).
The whole Stage 7 script also passed with `TMPDIR=/tmp/void-ci80-alias`.
`git diff --check` passed. No full native matrix was repeated. Local Linux evidence
does not claim native Windows/macOS execution of the repair. Stopped for owner
review/commit/push and cross-platform revalidation; no Git mutations, hosted
dispatches or publication. Unrelated website work preserved; Stage 7 is not locked.

## Stage 8 Sprint 1 engineering — 20 October 2026 (locally verified; hosted pending)

Boss authorized only project/module/IntelliSense discovery. Stage 7 is now
owner-supplied **HOSTED LOCKED** at `f704d75ffa37bfd0e587ec2b1513054e07a8a4d8`,
CI #81 attempt 2, seven successful jobs, 980/980 suites. Older pending statements
above remain historical. No assistant Git, hosted or publication operations occurred.

The existing parser/SourceGraph and compiler recovery query now implement nested
unquoted standard sources, importer-relative bare quoted project sources,
separate standard identities/library visibility, metadata-only file/folder menus,
source docs and installed-source navigation with readonly guards. Shared bounded
main discovery is exposed through voidls preflight and consumed by CLI/editor
adapters; current entry execution is preserved. See STAGE8_DISCOVERY.md for the
concrete engineering choices and stage8/ACCEPTANCE.md for final evidence/status.

Roadmap, approval ledger, Wrench handoff, strategy, decisions, known issues,
module/session/analysis/editor contracts, editor setup and README are synchronized.
Remaining sequence is (2) ordinary named/default calls and built-in APIs,
(3) UTF-8/recoverable collections, (4) optional-main/native providers,
(5) measured async retention, (6) selected ergonomics. Those sprints were not begun.
Final local evidence: **141/141 Debug (438.15 s), 141/141 Release (166.45 s),
141/141 strict ASan/UBSan/leaks (373.17 s), 423/423 total**. Focused checks passed
8/8; separate 1 MiB frontend gates passed Debug/Release. A final Windows-drive
known-standard-URI mapping repair received server rebuilds and 5/5 LSP suites per
configuration (15/15); its Windows branch still awaits hosted execution. Strict
sanitizer build/tests and Chromium ran outside tracing restrictions without
weakening flags. The old malformed `use bare` fixture now uses `use bare/`, since
bare standard imports are legal. Hard-link checks continue to use physical roots,
not overlay diagnostic labels; BOM import positions and quoted spaces are tested.

All three actual VS Code development profiles passed, including automatic menus
and file acceptance at every requested prefix. Actual Neovim 0.13 development,
portable 0.12.6 and Vim 9.2/pinned yegappan client passed shipped-adapter checks;
real terminal automatic menus/acceptance passed every prefix in both Neovim
versions and Vim. Automatic and explicit selection profiles passed on installed
Neovim/Vim. All 15 website gallery contracts checked and passed normal/O2 linked
AOT; 12 synchronous programs also passed normal/O2 JIT, while three async programs
used the established explicit async AOT adapter. The nested standard/project
fixture passed normal/O2 JIT/AOT and standard-source cache miss/hit/edit invalidation.

Metadata measurements at 10/1,000/10,000 siblings, 1,000 overlays and irrelevant
descendants justified query-local virtual-child maps; no persistent index was
introduced. Exact samples, commands, source hashes, client records, limitations
and surviving future decisions are in [stage8/ACCEPTANCE.md](stage8/ACCEPTANCE.md).
VSCodium, Windows/macOS, minimum clients, new installed packages and clean-machine
portability remain unverified. No known failing local test remains. Production
changes are ready for Wrench review. Sprint 1 is **not hosted locked**; Boss alone
controls Git, hosted CI and publication. Sprints 2–6 were not started.

## 20 October 2026: Stage 8 Sprint 1 CI #82 focused Windows repair

Inspected CI #82 Windows Release job 119004113561: `lsp_stage8` (#61/141)
failed with “Active file is outside selected workspace” on `RUNNER~1` temp paths.
Windows Debug job 119004113913 built successfully but was cancelled during tests;
it did not establish a pass for discovery. Repaired the shared frontend selection
helper to expand only the Windows workspace prefix, preserve descendant alias/case
checks and use canonical spelling for bounded ancestor traversal. Added process
coverage and physical-identity assertions in `tests/lsp/stage8.py`.

Focused Linux Debug and Release checks passed; see
[exact evidence and pending Windows gates](stage8/CI82_WINDOWS_REPAIR.md).
No full local/hosted matrix, commit, push, branch change or Sprint 2 was performed.
Root README was left unchanged.


## 20 October 2026: Stage 8 Sprint 2 calls and numeric prelude

**IMPLEMENTED / LOCALLY VERIFIED; STOPPED for Wrench review.** All three observable
policy decisions were explicitly approved by Boss and recorded in
[DECISIONS.md](DECISIONS.md). No pending Sprint 2 semantic choice is silently settled.

- Reused initializer argument mapping for ordinary functions, methods and generic
  inference. Preserved written evaluation order, readonly effects, staged
  construction, async/spawn argument mapping and existing constructors. Pure
  defaults support concrete annotation inference and explicit generic parameter
  types, with specialization context separate from runtime locals/receivers.
- Added only numeric min_value/max_value/abs/clamp across the ten numeric types.
  Type and receiver calls share canonical private typed IR implementations and
  existing checked failure handling, LLVM 19.1.7 JIT/AOT and exact numeric kernels.
  Width-checked numeric bitcast clears floating signs without losing NaN payloads.
- Compiler-owned metadata supplies completion, hover and signatures; synthetic
  prelude declarations never become file navigation, rename or symbol targets.
  Named ordinary parameter labels navigate and rename through real declarations.
  Parameter names/defaults participate in reload compatibility; body-only reload
  preserves existing leases. No overload resolution or primitive extensions added.
- Final frozen-source Debug **143/143**, Release **143/143**, strict
  ASan/UBSan/leaks **143/143**: **429/429, zero failures**. Both Debug and Release
  1 MiB frontend gates passed. Added focused 71-process normal/O2 JIT/linked AOT
  coverage, numeric native bit checks, reload/async/capability checks and LSP tests.
- Initial verification exposed an obsolete string-default rejection and a Debug
  recursive-frame regression; replaced the obsolete assertion with positive and
  negative coverage and outlined cold numeric helpers without weakening depth or
  stack guards. Strict sanitizer execution ran outside ptrace after LSan rejected
  tracing; instrumentation and leak detection stayed enabled.

[Contract](STAGE8_CALLS_NUMERIC.md), [acceptance](stage8/SPRINT2_ACCEPTANCE.md),
[results](stage8/sprint2-results.json), [source hashes](stage8/sprint2-source-hashes.json)
and [stack measurements](stage8/sprint2-stack-frames.json) retain exact evidence.
The CI #82 repair source/test hashes are unchanged. Root README is unchanged.
No hosted CI was run/requested; Windows Debug/Release repair execution, Windows/
macOS Sprint 2 acceptance and fresh real interactive editor checks remain unverified.
No commits, pushes, branch/remote changes or publication. Sprints 3–6 remain future
work; no automatic progression is authorized.


## 20 October 2026: Stage 8 Sprint 3 preparation and approval checkpoint

Boss explicitly authorized Sprint 3 from verified starting commit
`483ef87c3ec62c29c2bf1000529b480f0ba5ef2d`; the initial worktree was clean.
Read current contracts and inspected UTF-8 validation, string storage/publication,
collection mutation/versions, result representation, capabilities and tooling paths.
Presented one consolidated three-part public API approval checkpoint. Its proposed
text index/count, owned UTF-8 and recoverable collection contracts remain **pending
owner approval**, not implemented or accepted. See
[the preparation contract](STAGE8_TEXT_COLLECTIONS.md).

The existing Release build passed **7/7 focused baseline suites**, zero failures,
43.16 s. [Preparation evidence](stage8/sprint3-preparation.json) distinguishes this
from fresh implementation acceptance. No production code, root README, Git state,
remotes, hosted CI or website was modified. No Sprint 4 work. Continue Sprint 3
engineering only after the required public API decisions are approved; retain
Sprints 1/2, strict local acceptance and the deferred end-of-Stage-8 hosted gate.


## 20 October 2026: Stage 8 Sprint 3 approved implementation in progress

Boss approved all three public API groups after the consolidated checkpoint:
i64 scalar/byte counts, owned one-scalar checked indexing and owned UTF-8 byte
conversion; string-error result get/pop signatures, including fixed-array get.
[DECISIONS.md](DECISIONS.md) records the exact approval. The preparation-only
checkpoint entry above is superseded; no additional semantic decision is pending.

Engineering reuses existing strings, strict UTF-8 validation, typed buffers,
results, capability facts, checked failures and compiler-owned editor metadata.
Safe collection APIs lower through existing count/contains/read/remove/variant
operations; no collection convenience opcode or duplicate Runtime was added.
Focused Release process/native/LSP checks pass, including exact byte ownership,
malformed encodings, all integer widths, readonly effects, fixed copies, pop
versions, GC, allocation-failure cleanup/retry and compatible retained-lease reload.
Consolidated local builds/acceptance are in progress, not yet claimed complete.

An initial Debug 1 MiB rejected-chain check crashed because new result/text hint
temporaries expanded recursive frames. Cold helper boundaries repaired it without
changing nesting guards or stack reserves. Nominal/numeric/string legal and
recovering-rejection depth checks now pass; [measurements](stage8/sprint3-stack-frames.json)
retain the actual prologue evidence. Engineering fixture failures (unsupported
numeric suffix spelling, missing C++ include, omitted AOT Runtime link inputs,
stdout/stderr failure capture and allocation-probe frame reservation) were repaired
without weakening product checks. Final evidence must retain these distinctions
and count only completed final suites.

Root README and CI #82 repair source/tests remain unchanged. No Git/remote,
website or hosted CI mutations. Sprints 4–6 remain future work. Stop after
Sprint 3 local acceptance for Wrench review and Boss-controlled publication.


### Sprint 3 consolidated-test assertion repair

The first consolidated Release run passed **144/145**; arrays_foundation retained
an obsolete expectation that `return "x"[0]` in an i32 function was unsupported
syntax. Sprint 3 correctly returns string and diagnoses the incompatible i32
return as VOID2001. Updated that diagnostic expectation, retained the negative,
and added a positive string-returning scalar-index analysis. The focused Release
arrays_foundation rerun passed **1/1** (0.05 s). No production source changed.
Original failure evidence is retained; the rerun is one repaired suite, not an
additional unique suite. Debug/strict acceptance must use the repaired assertion.


## 20 October 2026: Stage 8 Sprint 3 local acceptance and Wrench handoff

**IMPLEMENTED / LOCALLY VERIFIED; STOPPED for Wrench review.** Delivered approved
scalar/i64 length and byte_length, owned scalar indexing/UTF-8 conversion and
string-error result list/fixed-array/dictionary get plus list pop. Reused the
existing Runtime, buffers, strict validator, results, capability checks, typed IR
and LLVM 19.1.7 JIT/AOT; no collection convenience opcode or extra allocator/cache.
Compiler-owned LSP metadata covers the APIs without fake writable navigation.

Final unique outcomes: **435/435**. Debug **145/145**; strict ASan/UBSan/leaks
**145/145**. Release **144/145**, followed by the documented test-only diagnostic
assertion repair and focused **1/1** arrays_foundation pass; no production code
changed for that repair. Reruns are not counted as extra unique suites. Both full
Debug/Release 1 MiB frontend gates passed. Strict execution stayed outside ptrace
with leak detection and halt-on-error enabled. Focused process coverage is 106;
GC/failure/copy/readonly/version/reload and compiler-owned editor tests passed.

[Acceptance](stage8/SPRINT3_ACCEPTANCE.md), [results](stage8/sprint3-results.json),
[source/test hashes](stage8/sprint3-source-hashes.json),
[stack evidence](stage8/sprint3-stack-frames.json) and
[traversal measurements](stage8/sprint3-traversal.json) retain exact scope, commands,
failures/fixes and limitations. All owner decisions are approved. Runtime-only
queries, builtin method values and conservative worker method-call admission are
explicit boundaries; no expansion is inferred. Wrench review, new actual editor
checks and Windows/macOS remain pending, including CI #82 Debug/Release repair
execution at the deferred end-of-Stage-8 hosted gate.

Sprint 1 repair hashes and Sprint 2 contracts are preserved. Root README unchanged;
no commits, pushes, branches/remotes, hosted CI, website or publication. Sprints
4–6 remain future work. Permanent Keyboard/Research/Wrench handoffs are under docs/.
