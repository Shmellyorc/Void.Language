# G76 — IntelliSense readability, responsiveness and incremental analysis

> Storage correction, 2026-11-21: the historical `/tmp` evidence locations in this report have been cleaned. Retained Markdown and original failure records now use the [verified migration inventory](/home/shmellyorc/Documents/Void.Language-archives/storage-correction-2026-11-21/MIGRATION_INVENTORY.md). Redundant generated outputs and disposable fixtures were deleted under the Boss’s updated retention instruction. Historical commands and measurements below are unchanged.

Boss authorized one integrated G76 milestone at baseline
`b777638c04834fc51bf81017802024cede43d041` on 18 November 2026.
Implementation and local Linux qualification are complete. This is implementation
evidence, not independent acceptance. No staging, commit, push, user-prefix
installation, extension publication or hosted CI was performed or is authorized.
G69 remains ON HOLD. Existing worktrees, archives and user settings are retained.

## Research and implementation decisions

- Roslyn's [completion handler](https://github.com/dotnet/roslyn/blob/main/src/LanguageServer/Protocol/Handler/Completion/CompletionHandler.cs)
  obtains a specific document and passes cancellation through completion services.
  Its [semantic model](https://github.com/dotnet/roslyn/blob/main/src/Compilers/Core/Portable/Compilation/SemanticModel.cs)
  retains semantic information for repeated questions. VOID reuses immutable
  analysis/catalog data with exact document revisions and fresh input admission;
  it does not adopt a second resolver or return unverified speculative bindings.
- Roslyn separates [symbol display rules](https://github.com/dotnet/roslyn/blob/main/src/Compilers/Core/Portable/SymbolDisplay/SymbolDisplayFormat.cs)
  from identity. Its [signature handler](https://github.com/dotnet/roslyn/blob/main/src/LanguageServer/Protocol/Handler/SignatureHelp/SignatureHelpHandler.cs)
  builds a label from display parts and reports the semantic argument index.
  VOID keeps its language's parameter/type syntax, shortens editor type paths
  only, and negotiates UTF-16 parameter-label offsets for precise highlighting.
- [clangd request scheduling](https://clangd.llvm.org/design/threads) keeps the
  protocol owner available while analysis runs and discards obsolete reads and
  superseded work. VOID uses bounded immutable assistance jobs, current-revision
  acceptance and cancellation replies; diagnostics remain separately debounced.
- [rust-analyzer's architecture](https://github.com/rust-lang/rust-analyzer/blob/master/docs/book/src/contributing/architecture.md)
  separates current inputs from derived semantic data. The applicable principle
  here is explicit invalidation and reuse. VOID does not add Salsa or another
  dependency, parser, language implementation or execution engine.
- The [LSP specification](https://microsoft.github.io/language-server-protocol/specifications/lsp/3.17/specification/)
  supplies versioned synchronization, cancellation, content-modified responses
  and signature label-offset negotiation. The updated extension requests
  incremental synchronization; servers that ignore that option keep advertising
  their existing full-sync capability. Legacy clients retain the original default.

These are existing upstream implementations/designs, not proposed upstream fixes
or evidence that their latency automatically transfers to VOID.

## Baseline and evidence

The LLVM/Clang/LLD 23.1.4 Release `voidls` and `voidc` targets were rebuilt before
measurement. Copies of both baseline binaries are retained. The representative
raylib fixture uses current verified source, a copied existing descriptor/catalog,
an explicit native requirement and a freshly verified lock. No native function
was executed and no catalog or installation was changed in place.

First fixture setup exposed two configuration errors: an extra generated main
caused VOID5010 ambiguity, then the application lacked its native requirement.
The fixture was corrected through ordinary manifest/lock operations; these were
not treated as compiler failures. Original diagnostics remain in the evidence.

Baseline Release, 25 requests per warm case, protocol round-trip wall time:

| Case | Median ms | p95 ms | Worst ms |
| --- | ---: | ---: | ---: |
| Warm namespace completion in an existing call | 170.22 | 176.70 | 220.82 |
| Incomplete standalone selection with newline | 183.65 | 193.93 | 199.19 |
| Incomplete selection at EOF | 177.86 | 185.86 | 186.54 |
| Immediate completion after EOF edits | 191.66 | 245.25 | 316.07 |

Initial cold initialization/open was 1,076.11 ms. These figures measure the
LSP client/server path, not rendered VS Code latency. The first result-reuse
candidate still took roughly 94–97 ms median; that unfavorable result is retained.
Opt-in phase profiling attributes roughly 65–74 ms per warm request to its two
dependency-verification passes, before response conversion. Detailed phase
durations are inclusive and must not be summed as exclusive CPU time.

Raw inputs, binaries, measurements, profiling output and intermediate build logs
remain under `/tmp/void-g76-56i5xfx0`. Selected original and final evidence is
preserved in [results.json](https://github.com/Shmellyorc/Void.Language/blob/df33af0a7e8ae75b97d2ab27395a683b69d3994f/docs/projects/evidence/g76/results.json),
[verification.txt](https://github.com/Shmellyorc/Void.Language/blob/df33af0a7e8ae75b97d2ab27395a683b69d3994f/docs/projects/evidence/g76/verification.txt) and the
[actual editor screenshot](https://github.com/Shmellyorc/Void.Language/blob/df33af0a7e8ae75b97d2ab27395a683b69d3994f/docs/projects/evidence/g76/workbench.png). Windows/macOS execution
remains unqualified.

## Implementation

`voidls` retains two revision-specific assistance results and their encoded JSON,
with one complete fresh input capture before a reused read. Project admission,
lock verification, directory/boundary inventory, all package input bytes and
native artifact bytes remain current. An immutable previously verified byte table
allows identical digest/image/descriptor work to be reused; matching size or
mtime alone is never sufficient. Closed local source files are recaptured rather
than hidden behind historical overlays. Normal compilation uses the original
resolver entry point without an editor reuse hint.

Namespace completion uses the ordinary lexer/parser to prove expression context
and unchanged declarations around body edits. Imports, signatures, globals, type
contexts, strings/comments and possible alias shadows delegate to ordinary
analysis. Recognition of an empty matching namespace is distinct from failure to
prove reuse. No alternative language parser or symbol resolver was added.

Expensive uncached assistance uses a bounded queue (64 accepted requests, one
running assistance job) and immutable snapshots. The protocol owner handles
newer edits and cancellation while work runs. Queued obsolete work is skipped;
an already running compiler operation finishes safely, but cannot publish an
obsolete reply. Background diagnostics retain their existing debounce and
full-project analysis. This is selective incremental reuse, not a claim of a
fully incremental semantic compiler. An unrelated cold probe can still delay the
next uncached job until that bounded running operation finishes.

The extension negotiates UTF-16 incremental edits; full replacements and legacy
clients remain supported. Multi-event updates commit atomically, including
surrogate-pair/range validation. Presentation responses are checked against the
requesting document version and current project. A public language-client
notification registration handles versioned diagnostics before initialization;
conversion results cannot overwrite newer notifications, edits, closed/reopened
documents or a replaced client. No private language-client API is used.

Callable details retain parameter/return types and all overload/specialization
headers, while editor type display removes internal source-path qualifiers.
Authored documentation and the existing active-documented-parameter policy are
retained. Parameter highlighting negotiates UTF-16 label offsets. Compiler symbol
identities, navigation locations, source compatibility, native ABI, execution and
LLVM passes are unchanged. Complete tokens before an edit retain their semantic
role, including tokens earlier on the edited line; extended partial identifiers
are withheld until resolved.

## Rapid typing investigation and unfavorable evidence

The first real-editor candidate improved warm provider time but regressed rapid
`rl.` popup paint: 367.70 ms median versus 324.05 ms baseline. It was rejected.
An actual request profile isolated repeated `assistance/analysis` work for EOF
selections: 188.18 ms median versus about 20 ms for the newline-terminated case.
The declaration fingerprint was treating the lexer's synthetic EOF newline as a
header change. Closing body dedents were excluded, but that newline was not.

The correction classifies both closing body tokens as body trivia at that exact
boundary, retaining the EOF token and all declaration comparisons. Direct tooling
coverage requires the reuse API itself to recognize EOF/newline, assignment,
nested-call and subsequent body edits; merely obtaining correct completion via a
slow full-analysis fallback cannot pass it. Shadowed aliases, changed headers or
imports and non-expression contexts must delegate. The instrumented EOF median
then fell to 22.69 ms. That small run overlapped builds and is mechanism evidence,
not the final latency qualification.

Roslyn's merged, current [incremental parser implementation](https://github.com/dotnet/roslyn/blob/main/src/Compilers/CSharp/Portable/Parser/Blender.cs)
tracks affected text ranges and retains unaffected syntax, including careful
handling of zero-width tokens and end boundaries. Its completion handler uses
async document operations, cancellation and cached completion lists. These support
VOID's structural reuse and responsive protocol design; they do not establish an
identical C# defect or justify reusing stale semantic results. Async removes
protocol blocking for expensive probes; eliminating unnecessary work remains
necessary for fast typing.

Original focused failures were retained and corrected: generic template parameter
display initially requested an unavailable instantiated array type; an extra
signature description violated the existing active-parameter prose contract; an
intended hover golden needed the shorter public type. The first integrated
Release run was 52/54 because compact details hid random overloads and selected
operator specializations. Compact display now retains every callable header;
those existing tests and the focused re-run pass. Initial test-fixture setup and
a C++ regression-fixture initializer-list build error are retained separately
from production failures. A renderer run overlapping a strict build had provider
p95 97.12 ms and is retained as a contended observation, not silently substituted
for the final quiet run.


## Final performance qualification

Release binaries, LLVM 23.1.4, native Linux x86_64; no builds or regression jobs
ran alongside the final series. Each protocol case has 100 measured requests,
following a separately reported first request and five warmups. p95 uses nearest
rank; worst and all raw samples are retained. Timing uses monotonic wall clocks.
Every response must contain the expected member; no partial/incorrect result is
accepted to achieve a latency. Normal detailed instrumentation is disabled.

| Protocol case | Baseline median / p95 / worst ms | G76 median / p95 / worst ms |
| --- | ---: | ---: |
| Raylib warm existing call | 173.71 / 182.62 / 214.19 | 20.81 / 21.48 / 22.28 |
| Raylib selection with newline | 177.97 / 186.92 / 225.28 | 20.60 / 21.14 / 26.55 |
| Raylib selection at EOF | 178.79 / 193.76 / 252.87 | 20.60 / 21.41 / 25.64 |
| Immediate unsaved EOF edits and prefixes | 173.87 / 190.27 / 202.98 | 18.17 / 41.42 / 43.19 |
| Small local-module warm call | 0.92 / 0.95 / 0.98 | 0.45 / 0.50 / 0.64 |
| Small local-module edited EOF | 0.96 / 0.98 / 0.99 | 0.51 / 0.54 / 0.57 |

Raylib initialize/open: 1,071.33 ms baseline and 995.03 ms G76; small project:
27.26 and 28.38 ms. These are individual cold observations, not cold percentiles.
First G76 raylib requests for call/newline/EOF were 61.43/40.47/38.50 ms, separate
from warmed samples. Cold opens and substantial declaration/project changes have
not been represented as warm-cache performance.

With `VOID_LSP_PROFILE=1`, the same 100-sample raylib series reported warm-call
20.01 / 21.01 / 24.23 ms and edited-EOF 17.26 / 39.18 / 40.48 ms. Instrumentation
is explicitly opt-in and stderr-only. The slightly lower later numbers are run
variation/cache effects, not evidence of negative instrumentation overhead. Both
settings meet the warm LSP p95 target on these fixtures. Remaining raylib cost is
dominated by fresh project/dependency admission; this implementation does not
trade those checks for a timestamp-only shortcut to chase the 10 ms stretch.
The small fixture exceeds that stretch goal. No universal latency guarantee or
large-workspace scaling claim follows from two representative projects.

VS Code **1.144.0**, isolated development host/profile, current extension source,
explicit rebuilt server/compiler paths; no installed extension or user settings
changed. Provider: 50 samples after five warmups; rapid typing: eight `r`, `l`, `.`
command sequences into the actual editor; assignment edits: 15. Renderer
MutationObserver timestamps record popup DOM visibility and two animation frames
record subsequent paint. The 100 ms observer-result retrieval polling is not
included as a measured delay. Frame sampling independently records an unchanged
call's computed foreground during typing.

| Real editor case | Baseline median / p95 / worst ms | G76 median / p95 / worst ms |
| --- | ---: | ---: |
| Warm completion provider | 184.00 / 190.94 / 194.48 | 38.85 / 46.18 / 100.66 |
| Rapid rl. popup DOM | 304.25 / 323.70 / 323.70 | 186.00 / 206.50 / 206.50 |
| Rapid rl. subsequent paint | 326.30 / 345.80 / 345.80 | 211.60 / 236.80 / 236.80 |
| Assignment edit through correct provider result | 201.38 / 227.81 / 227.81 | 90.71 / 115.49 / 115.49 |

These editor measurements include command dispatch, client conversion, scheduling
and rendering. They are not server timings; painted popup latency is not claimed
to be under 50 ms. The provider's single 100.66 ms worst sample is retained.
All eight G76 rapid sequences retained the unchanged `color` call's foreground
(`rgb(210, 168, 255)`) across 110 sampled frames. This checks a representative
unchanged call, not every possible token or theme. Real editor checks also
observed an incomplete-call error, its clearance after repair, hover, semantic
tokens and `fade`'s second argument selected through precise label offsets.

The final baseline-editor probe intentionally used the same new assertions. Its
common performance phases completed, then it failed the new precise-parameter-
offsets assertion because the older server supplies string labels. That original
FAIL and its error are retained; it is before/after evidence of the new contract,
not a claimed baseline acceptance pass. The G76 editor probe passed all checks.

Interleaved raylib CLI `check --project ... --status off`, 20 samples per binary
after two warmups: baseline median/p95 460.71/481.13 ms; G76 470.68/487.48 ms.
The +9.97 ms (+2.16%) median is reported rather than hidden; this fixture shows
no substantial compiler regression. Exact verified input tables retain additional
bytes per held analysis; peak memory and large-project scaling were not separately
qualified. Normal compiler execution and optimization settings are unchanged.

## Correctness qualification

All relevant test executables were rebuilt/relinked against the changed source.
The initial 72-test Debug/Release gates were followed by 15 refreshed linked-test
and dependency/selection checks: **80 distinct tests per profile**, not 87.
Strict ran the combined selection once: **77/77**. No expected-failure
classification, test timeout, assertion, stack limit or sanitizer policy changed.

| Gate | Result |
| --- | --- |
| Native Linux Release relevant integrated tests | **80/80 distinct PASS** |
| Native Linux Debug relevant integrated tests | **80/80 distinct PASS** |
| Fatal ASan/UBSan + leak checks | **77/77 PASS** |
| Extension TypeScript build and unit tests | **13/13 PASS** |
| Registered G76 process test + direct tooling reuse test | PASS in all three profiles |
| Native catalog tampering/freshness | **5/5 inputs rejected and restored**, each profile |
| Cancellation after an observed worker-start marker | **8/8 in-flight cancellations**, each profile |
| Actual VS Code G76 provider/render/diagnostic checks | PASS |

The 1 MiB `text_encoding_semantic_depth` test passed in all profiles, preserving
400-call receiver rejection, 80-level defer/closure rejection, accepted legal
depths, source locations, codes and recovery. Existing `source_cli` passed too.
Strict retains its existing exclusions of `lsp_const_recovery_stack`,
`lsp_io_stack` and `lsp_text_encoding_stack`; these three passed in Debug and
Release and are not claimed as strict passes. Fatal environment:

```text
ASAN_OPTIONS=detect_leaks=1:halt_on_error=1:abort_on_error=1:exitcode=87
UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1:exitcode=86
```

Qualification includes LSP framing/positions/cancellation, completion overloads,
documentation, named arguments, semantic recovery and project changes; package,
Git/HTTPS, native approval/identity, selection/profile and manifest tests. The
sentinel allocation-elision JIT and both normal/optimized AOT tests passed, as did
`project_managed_native`, `project_manifest_legacy` and `project_mixed_native`.
Their existing expected negative/linker and application-exit assertions remain
intact. All five native input mutations retain size and mtime, including notices
and the catalog; no native function is executed by that focused metadata probe.
Cancellation reply worst across the final 24 observed-running trials was 0.25 ms.
Those probes enable start-marker profiling to establish actual in-flight work;
they are not the instrumentation-disabled latency benchmark.

Focused coverage also checks closed local-source edits and non-imported package
README edits with restored mtimes, unchanged canonical navigation, UTF-16 emoji
and CRLF edits, atomic rejection of malformed multi-edit updates, unchanged-line
semantic tokens, request-ID reuse, queued stale revisions, project selection and
EOF/shutdown response retirement. Diagnostic conversion tests cover out-of-order
completion, edit, close/reopen and legacy unversioned notifications.

The same ordinary application returns **19** in baseline/G76 JIT O0 and O2.
Original emitted object hashes differed. LLVM objdump isolated only
`.rodata.str1.1..Lvd.application.provenance.v1`, whose compiler-build inputs are
intentionally distinct (`application_build_provenance`, unchanged backend).
Removing only that section from separate comparison copies produced byte-identical
objects in each optimization mode, including code and relocations. Original
objects, dumps and unfavorable comparison diffs were retained. Existing tooling
checks validate lowered IR and unchanged source-compatibility contracts. No LLVM
pass, native ABI, language semantics or security authorization changes were made.

## Reproduction and limits

The prepared raylib project needs verified raylib source, its ordinary dependency
lock and an existing supported native catalog. Qualification copied an existing
catalog into temporary fixtures; it did not download/install a replacement or
grant execution consent. Use a fresh output directory:

```sh
python3 tools/bench/intellisense.py --server build/llvm23-release/voidls \
  --project /path/to/prepared-raylib-project --document src/app.void \
  --namespace rl --expected clear_background --samples 100 --output /tmp/fresh-g76-run
```

Set `VOID_LSP_PROFILE=1` for a separate detailed run. For the editor harness, set
absolute `VOIDLS_PATH`, `VOIDC_PATH`, `VOID_G76_TEMPLATE`,
`VOID_G76_EDITOR_OUTPUT` (fresh) and `VSCODE_EXECUTABLE_PATH`, then invoke:

```sh
node -e "require('./tools/bench/intellisense_editor.cjs').launch().catch(e=>{console.error(e);process.exitCode=1})"
```

The integrated CTest selection is preserved in the evidence and can be reproduced
with this regex (existing build-specific registrations/exclusions still apply):

```text
lsp|tooling|project_protocol|project_session|^source_cli$|^text_encoding_semantic_depth$|^project_.*(native|package|manifest|resolution)|^allocation_elimination|^project_(dependencies|dependency_explain|git_dependencies|git_https|selection|profiles|discovery_cli|nested_cli)$
```

This is relevant integrated qualification, not a claim to have rerun all compiler
or Runtime tests. Native Windows and macOS editor/compiler execution are
**UNQUALIFIED**. Linux results and source inspection cannot replace those tests.
Actual project-switch correctness is covered by process tests; rendered timing is
for the representative raylib project, not a measured project-switch percentile.
Background diagnostics still perform debounced full analyses. Canceled running
work is safely retired for publication, not forcibly interrupted inside compiler
or filesystem code. G69 remains on its existing HOLD; no other milestone began.

`git diff --check` and separate whitespace checks of every new text file passed.
No staged changes, commits, pushes, installations,
publishing, worktree changes or hosted CI were performed.

## Original changed-file inventory (before the freshness correction)

26 paths: 15 modified and 11 new.

```text
CMakeLists.txt
docs/EDITOR_INTEGRATION.md
docs/README.md
editors/vscode/package.json
editors/vscode/src/extension.ts
include/void/frontend.hpp
include/void/project_resolution.hpp
src/frontend/compiler.cpp
src/frontend/project_native.cpp
src/frontend/project_native.hpp
src/frontend/project_resolution.cpp
src/frontend/project_selection.inc
src/lsp/main.cpp
tests/lsp/editor_recovery.py
tests/tooling/tests.cpp
docs/projects/G76_INTELLISENSE.md
docs/projects/evidence/g76/results.json
docs/projects/evidence/g76/verification.txt
docs/projects/evidence/g76/workbench.png
editors/vscode/src/diagnosticVersions.ts
editors/vscode/src/test/diagnosticVersions.test.ts
editors/vscode/src/versionedDiagnostics.ts
tests/lsp/intellisense.py
tests/lsp/intellisense_native.py
tools/bench/intellisense.py
tools/bench/intellisense_editor.cjs
```

## Async publication freshness correction — 19 November 2026

Golden Wrench's subsequent review returned CORRECTION REQUIRED for one P1. The
26-path inventory and qualification above describe the original candidate. Its
measurements, original 100.66 ms editor outlier, failures, hashes and evidence
remain preserved; they are not relabeled as measurements of the correction.
The Boss authorized this bounded correction separately.

### Root cause and corrected ownership

`run_idle_work` checked final filesystem inputs only if the job's document text
matched its historical successful analysis. Edited-document and no-history jobs
could send their results without that check. Golden Wrench stopped the worker
publication path before the conditional check, changed a closed field `x` to `y`
with size/mtime preserved, and observed stale `x` from the edited branch. The
unchanged-document control correctly returned `-32801`. Original review,
reproducer, debugger logs and results remain at
`/tmp/g76-independent-hzvx2j2t`; the original Debug and Release server binaries
were also preserved before rebuilding for this correction.

The repaired frontend attaches an owned `inputs_current` check to each probe.
It captures immutable source snapshots, frozen overrides, manifest bytes,
verified dependency/native ownership, discovery observations, directories and
boundaries before binding can unwind through `EditorProbeFound`. The existing
`ProjectCapture` and `SourceGraph` perform readmission and exact-byte comparisons;
there is no second parser, guessed timestamp identity or unrelated optimizer work.
Source snapshots share immutable byte storage; the checks retain no parser ASTs
or references to temporary capture objects. Import queries additionally recheck
their metadata-only candidate query and normal path admission.

Every completed async result must pass that job-owned check before serialization,
regardless of document edits or historical analysis availability. Async jobs no
longer retain a historical-analysis pointer. Reused async results keep their own
check; the already-verified namespace fast path retains its existing fresh-input
and parser-backed context checks. Root/revision/cancellation checks remain in
place. Failure retires the result with `-32801`, invalidates obsolete assistance
and refreshes analysis so a subsequent request can recover. The old unchanged
branch could reject repeatedly without initiating that refresh; this was also
observed in the preserved before-repair probe.

The repaired source overlay freezes probe coordinates. A target that was closed
at request time must additionally match its actual disk bytes; it cannot acquire
open-document authority merely because the compiler inserts a temporary hole.
Open documents retain their frozen editor overrides and document-version checks.

An input set that cannot be admitted, including an unlocked changed package,
cannot produce a verified async success. Two existing negative assertions now
require `-32801` rather than an empty successful response. They still reject all
suggestions from the invalid package; they do not relax a diagnostic, source
language, CLI/application exit, native approval or artifact contract.

### Upstream applicability

The current [Roslyn request-context framework](https://github.com/dotnet/roslyn/blob/main/src/LanguageServer/Microsoft.CommonLanguageServerProtocol.Framework/README.md)
requires handlers to use snapshots gathered by the serialized context factory,
rather than look up mutable document state during asynchronous work. Its
[completion handler](https://github.com/dotnet/roslyn/blob/main/src/LanguageServer/Protocol/Handler/Completion/CompletionHandler.cs)
threads cancellation through document/completion operations and caches results.
[clangd's scheduler](https://clangd.llvm.org/design/threads) likewise associates
work with expected file versions and discards obsolete reads. These current
implementations informed request-owned input provenance and conditional
publication. Their frameworks were not imported, and no claim is made that they
have an identical bug. C++ execution speed alone does not remove filesystem,
client-conversion or renderer scheduling costs.

### Deterministic regression coverage

`tooling` now exercises owned completion and signature inputs in both legacy and
manifest projects, without supplying historical analysis. The caller's temporary
unsaved override is destroyed before checking the returned proof. It checks
unchanged acceptance, same-size/same-time dependency mutation rejection, fresh
member recovery and exact-byte restoration. These checks run under the registered
portable C++ test in Debug, Release and fatal sanitizer profiles.

`tests/lsp/assistance_publication.py` is an additional native-Linux debugger
qualification. It uses an already available GDB against its own child, stops
**before** publication validation, changes a disposable input, and detaches before
shutdown/leak checks. Readiness markers control debugger attachment; there is no
sleep-based attempt to win the filesystem race and no production test hook.
Completion and signature requests cover unchanged, edited and no-history states;
a seventh completion case changes a closed target from one valid receiver type
to another. The eighth replaces an identically spelled/byte-identical source
with a hard-link alias before publication. It requires continued rejection while
the alias exists and successful recovery after distinct ownership is restored.
Every other rejection must be followed by a correct fresh result.

The new script reproduced the original stale `x` failure against the preserved
Debug binary. A separate original run also exposed its failed fresh retry after
the unchanged control. A first harness setup failure (copy lacked executable mode)
and an initial signature fixture that placed the cursor inside a numeric literal
were retained and corrected separately from the production defect. The first
integrated run also retained the old empty-success assertion failure before the
negative package expectations were tightened.

Reproduce the actual server test with a fresh output directory and an existing
GDB on native Linux:

```sh
python3 tests/lsp/assistance_publication.py \
  --server build/llvm23-debug/voidls --compiler build/llvm23-debug/voidc \
  --output /tmp/fresh-g76-publication-review
```

The standalone debugger test is not a new compiler/runtime dependency or a claim
of debugger availability in every CI/container environment. Its native Debug
server qualification complements the portable registered input-ownership tests.

### Earlier correction qualification and measured impact

This historical checkpoint predates the final identity/recovery and reuse work.
Its recorded source/binary hashes remain in the nested earlier evidence; the
final-source qualification below supersedes its gate counts.

Checkpoint source, LLVM/Clang/LLD 23.1.4, native Linux:

| Correction check | Result |
| --- | --- |
| Release relevant LSP/tooling/session/depth gate | **53/53 PASS** |
| Debug same relevant gate | **53/53 PASS** |
| Fatal ASan/UBSan + leaks, existing strict registrations | **50/50 PASS** |
| Deterministic actual Debug server publication cases | **7/7 rejected changes and fresh recovery PASS** |
| Portable owned-input regression (`tooling`) | PASS in all three profiles |
| Additional unsaved new-import/context regression | PASS in all three profiles |
| Native same-size/same-time tampering | **5/5 rejected**, each profile |
| Observed-running cancellation | **8/8 canceled**, each profile |
| Real VS Code corrected server harness | PASS |

The registered G76 test additionally changes an unsaved document to a newly
introduced source import, obtains its new receiver member, and restores the
previous context. It proves that ownership checks do not indiscriminately compare
legitimate new contexts against an old successful graph. It was added after the
integrated gates and rerun as a focused test in each profile; other unchanged gates
were not repeated unnecessarily. All test executables used by the integrated
selection were rebuilt/relinked against the correction.

The unchanged **1,048,576-byte** `text_encoding_semantic_depth` test passed in
Debug, Release and strict, including the original receiver/defer cases. The three
existing strict LSP small-stack exclusions remain excluded. Fatal ASan/UBSan and
leak policy is identical to the environment recorded above. These are relevant
correction gates, not a rerun or new claim about all compiler/Runtime suites.
The prior 80/80, 80/80, 77/77 and extension 13/13 results remain historical evidence;
this correction changed no extension source or settings.

Before/after performance compares the preserved **pre-correction G76** server
with the repaired server, separately from the much older pre-G76 baseline. No
builds or regression jobs overlapped these final runs; detailed profiling was
disabled. Warm protocol cases use 100 samples after the same first-request/five-
warmup separation. All raw samples and binary identities are preserved.

| Raylib protocol case | Before median / p95 / worst ms | Corrected median / p95 / worst ms |
| --- | ---: | ---: |
| Warm existing call | 20.30 / 22.90 / 25.70 | 20.35 / 21.37 / 23.33 |
| Warm newline selection | 20.68 / 24.02 / 26.98 | 20.33 / 22.29 / 24.63 |
| Warm EOF selection | 20.03 / 21.45 / 26.37 | 20.50 / 21.44 / 25.43 |
| Immediate edited namespace EOF | 16.96 / 39.11 / 41.56 | 17.93 / 40.55 / 44.30 |
| Uncached edited value-member completion | 176.34 / 196.84 / 201.73 | 192.03 / 217.61 / 234.41 |
| Uncached edited signature help | 181.29 / 191.26 / 191.38 | 197.90 / 207.52 / 207.82 |

The last two rows explicitly force a new unsaved body revision for every request,
after a settled analysis. They use 20 samples after three warmups and require
correct color receiver members / active `fade` parameter information. They do
not take the imported-namespace fast path or reuse a previous result. Input
capture/readmission adds about **15.69 ms (+8.90%)** to the member median and
**16.61 ms (+9.16%)** to signature help. These are whole-request comparisons, not
invented exclusive phase durations. This necessary correctness cost is retained;
uncached edited requests are not presented as warmed sub-50-ms responses. Warm
namespace median changes by only **0.05 ms** and the warm p95 improvement remains.

Same isolated VS Code 1.144.0 harness and sampling policy as the original record:

| Actual editor case | Before median / p95 / worst ms | Corrected median / p95 / worst ms |
| --- | ---: | ---: |
| Warm provider | 38.69 / 43.51 / 49.56 | 38.43 / 43.51 / 60.15 |
| Rapid rl. popup DOM | 190.65 / 226.10 / 226.10 | 195.80 / 218.40 / 218.40 |
| Rapid rl. two-frame paint proxy | 212.25 / 248.40 / 248.40 | 225.50 / 245.60 / 245.60 |
| Assignment edit through correct provider result | 94.92 / 135.07 / 135.07 | 95.36 / 114.56 / 114.56 |

Both editor runs passed correctness, invalid-to-valid diagnostics, precise active
parameter, hover and sampled unchanged-call coloring checks. The eight-run popup
median varied upward by 13.25 ms while p95 was slightly lower; neither that
variation nor the new 60.15 ms provider worst is hidden. The original **100.66 ms**
outlier remains in its untouched evidence. No universal rendering deadline or
claim that additional threads alone remove input/editor cost is made.

Selected original failures, checkpoint logs, debugger results, native checks,
complete samples and corrected source/binary hashes are preserved in
[freshness-results.json](https://github.com/Shmellyorc/Void.Language/blob/df33af0a7e8ae75b97d2ab27395a683b69d3994f/docs/projects/evidence/g76/freshness-results.json) and
[freshness-verification.txt](https://github.com/Shmellyorc/Void.Language/blob/df33af0a7e8ae75b97d2ab27395a683b69d3994f/docs/projects/evidence/g76/freshness-verification.txt). Full raw
before/after evidence remains at `/tmp/void-g76-freshness-j_ii4ckz`. The original
Golden Wrench evidence and prior G76 evidence have not been removed or overwritten.

### Correction and Roslyn-review file scope and current inventory

The correction and bounded Roslyn review touch these **13 paths**, including report/evidence:

```text
include/void/frontend.hpp
include/void/compiler_observation.hpp
tools/bench/intellisense_assistance.py
src/frontend/compiler.cpp
src/frontend/project_selection.inc
src/lsp/main.cpp
tests/tooling/tests.cpp
tests/lsp/editor_recovery.py
tests/lsp/intellisense.py
tests/lsp/assistance_publication.py
docs/projects/G76_INTELLISENSE.md
docs/projects/evidence/g76/freshness-results.json
docs/projects/evidence/g76/freshness-verification.txt
```

Native Windows/macOS execution remains **UNQUALIFIED**. The deterministic debugger
publication probe ran against native Linux Debug; the portable ownership and
normal process/cancellation checks ran in all three profiles. Peak memory and
large-workspace scaling limitations remain unchanged. Source semantics, LLVM
passes, native ABI, execution consent, application exit behavior and installation
state were not changed. No new dependencies, milestones, staged changes, commits,
pushes, installations, extension publication or hosted CI were introduced.
`git diff --check` and checks of new text files pass. G69 remains ON HOLD.
Final-source qualification and the recommendation are recorded below. This
implementation report does not claim independent acceptance.

Current complete candidate: **31 paths** (16 modified, 15 new):

```text
CMakeLists.txt
docs/EDITOR_INTEGRATION.md
docs/README.md
editors/vscode/package.json
editors/vscode/src/extension.ts
include/void/frontend.hpp
include/void/compiler_observation.hpp
include/void/project_resolution.hpp
src/frontend/compiler.cpp
src/frontend/project_native.cpp
src/frontend/project_native.hpp
src/frontend/project_resolution.cpp
src/frontend/project_selection.inc
src/lsp/main.cpp
tests/lsp/editor_recovery.py
tests/tooling/tests.cpp
docs/projects/G76_INTELLISENSE.md
docs/projects/evidence/g76/freshness-results.json
docs/projects/evidence/g76/freshness-verification.txt
docs/projects/evidence/g76/results.json
docs/projects/evidence/g76/verification.txt
docs/projects/evidence/g76/workbench.png
editors/vscode/src/diagnosticVersions.ts
editors/vscode/src/test/diagnosticVersions.test.ts
editors/vscode/src/versionedDiagnostics.ts
tests/lsp/assistance_publication.py
tests/lsp/intellisense.py
tests/lsp/intellisense_native.py
tools/bench/intellisense.py
tools/bench/intellisense_assistance.py
tools/bench/intellisense_editor.cjs
```

## Roslyn optimization review — 19 November 2026

This is part of G76 and preserves the freshness repair. Research inspected current
Microsoft source rather than inferring C# behavior from its use of `async`.
The observed Roslyn `main` head was
[`1d84ece6553b9fe2f5ea400ccad408f6ebf5b9f0`](https://github.com/dotnet/roslyn/commit/1d84ece6553b9fe2f5ea400ccad408f6ebf5b9f0).
This is a source inspection, not a Roslyn build or a cross-language benchmark.

| Topic | Confirmed upstream implementation | Applicability to VOIDLS |
| --- | --- | --- |
| Incremental semantic models | Roslyn's [Document.cs](https://github.com/dotnet/roslyn/blob/main/src/Workspaces/Core/Portable/Workspace/Solution/Document.cs), `GetSemanticModelHelperAsync`, returns a model cached on that document, otherwise obtains the document tree and project compilation and retains the model weakly. | VOID already retains immutable compiler analysis and selected assistance results. A result must remain tied to its actual inputs; an old successful analysis cannot validate a different job. |
| Compilation snapshots | [RegularCompilationTracker](https://github.com/dotnet/roslyn/blob/main/src/Workspaces/Core/Portable/Workspace/Solution/SolutionCompilationState.RegularCompilationTracker.cs) forks state and applies/merges translation actions while retaining reusable compilation state. | VOID's session revisions and owned source overlays provide the snapshot boundary. This review adds no alternative project graph or semantic analyzer. |
| Completion caches | [CompletionListCache](https://github.com/dotnet/roslyn/blob/main/src/LanguageServer/Protocol/Handler/Completion/CompletionListCache.cs) has three entries for completion-to-resolve reuse. It is not an unconditional cross-edit completion cache. | VOID's two selected-result entries remain bounded and freshness checked; serialized result reuse avoids repeated presentation work. |
| Dependency invalidation | [Project.cs](https://github.com/dotnet/roslyn/blob/main/src/Workspaces/Core/Portable/Workspace/Solution/Project.cs) exposes dependent semantic versions; the [incremental parser design](https://github.com/dotnet/roslyn/blob/main/docs/compilers/Design/Incremental%20Parser.md) explains immutable reusable nodes and context-sensitive invalidation. | VOID uses compiler-token header comparisons for the namespace fast path, and exact reads plus normal filesystem admission for dependencies. Workspace version stamps alone would not detect VOID's same-size/same-time closed-file mutation case. |
| Async scheduling | [RequestExecutionQueue](https://github.com/dotnet/roslyn/blob/main/src/LanguageServer/Microsoft.CommonLanguageServerProtocol.Framework/RequestExecutionQueue.cs), especially context creation and mutating-request handling, captures context serially before allowing nonmutating handlers to run concurrently. Cancellation is explicit. | VOID already captures a session snapshot before launching its bounded compiler worker. Canceled or superseded jobs cannot publish. Async maintains responsiveness; it does not remove the CPU cost of analysis. |
| Incomplete code | [CompletionService_GetCompletions](https://github.com/dotnet/roslyn/blob/main/src/Features/Core/Portable/Completion/CompletionService_GetCompletions.cs) selects frozen partial semantics and shares syntax contexts/speculative models among providers. | VOID keeps compiler-owned cursor repair and recovery, including `fu` to `func`, and a conservative namespace fast path. Roslyn's deliberate partial-semantics policy is not permission to return stale VOID dependency members or bypass native admission. |
| Instrumentation | [QueueItem](https://github.com/dotnet/roslyn/blob/main/src/LanguageServer/Microsoft.CommonLanguageServerProtocol.Framework/QueueItem.cs) starts request telemetry at enqueue/context execution; [RequestTelemetryLogger](https://github.com/dotnet/roslyn/blob/main/src/LanguageServer/Protocol/Handler/Telemetry/RequestTelemetryLogger.cs) records queue and execution durations separately, plus outcome counts. | VOID records protocol round trips and actual editor results separately. Opt-in compiler observations now also expose freshness, candidates, formatting and publication without changing CLI timing phases or protocol results. |

Roslyn's [#70609 discussion](https://github.com/dotnet/roslyn/issues/70609) identifies
nullable-flow work unnecessary for a particular classification query. Its
[implementation #71036](https://github.com/dotnet/roslyn/pull/71036) is **merged**;
the issue remains open at inspection. This supports measuring the work a feature
actually needs, not disabling VOID correctness checks. The historical allocation
[issue #23582](https://github.com/dotnet/roslyn/issues/23582) recommends repeatable
Release profiling; it is closed as not planned, not a newly merged LSP fix.
[clangd's threading design](https://clangd.llvm.org/design/threads) similarly
separates the protocol thread from compiler workers and cancels obsolete reads.
Its stale-preamble policy is not adopted for VOID's dependency-security boundary.

### Measurement and bounded optimization

Before changing reuse behavior, `VOID_LSP_PROFILE=1` measured the existing resolved
raylib fixture. Across 46 uncached worker probes, median inclusive times were:
worker total **177.05 ms**, project capture **76.08 ms**, dependency verification
**70.30 ms**, lex/parse **30.33 ms**, semantic analysis **46.72 ms**, candidate work
**12.37 ms** and compiler signature formatting **0.002 ms**. Final publication
validation took **15.51 ms**, formatting **0.033 ms**, and stream publication
**0.010 ms**. Warm completion handler median was **16.33 ms**, including freshness
**15.71 ms**, dependency verification **12.56 ms**, and publication **0.247 ms**.

These are observed **inclusive**, potentially nested intervals and medians across
requests, not a sum of exclusive phase costs. Candidate intervals can contain
formatting. Worker and publication are separately scoped; protocol round trips
also include queueing, framing and scheduling. Profiling includes warmups, while
normal latency tables discard the documented warmups. An absent phase means no
scope was entered, not an invented zero-duration compiler phase.

The measured repeated work was two full native/dependency admissions inside an
uncached job: the initial capture and the cursor-repaired compiler capture. The
small correction passes the first capture's immutable `LockedDependencies` to
the second as the **existing byte-verified reuse hint**. Both captures still
reread the lock, package/native inputs and check catalog/root/file identities.
Only deterministic derived metadata is reused after the normal exact-byte match.
The final job-owned readmission remains mandatory. The change is internal to the
frontend helper and repaired-document capture; CLI callers default to no hint.
There is no persistent new cache, extra project model or second semantic engine.

Additional profiling uses the existing thread-local observation sink with four
fixed editor-detail records. It is opt-in, stderr-only and does not add phases to
the versioned CLI JSON schema. Same-kind nesting is counted once inclusively;
worker and publication bindings are invocation-local. Ordinary measurements have
profiling disabled. The new `tools/bench/intellisense_assistance.py` reproduces
warm/edited value-member and signature probes against an isolated raylib copy,
with three warmups and median/p95/worst/raw samples retained even after failure.

Hard-link qualification retains real compiler identity checks: fresh captures
record their recaptured source owners before resolving subsequent modules.
Without those owner entries the loader's filesystem-equivalence comparison had
no earlier module to compare against. The original pre-repair hard-link server
failure is retained in `before-hard-link`, and the focused C++ ownership tests
cover both legacy and manifest modes. The added package-manifest hard-link test
rejects the old cached result, verifies its boundary diagnostic, admits no package
suggestions while selection is invalid, and recovers after removing the alias.
Its first test draft incorrectly required an async error after invalid selection
had already rejected the request before scheduling; that failed assertion and
its empty result are retained in `roslyn-package-hardlink-debug.log`.

An intermediate strict freshness patch also rejected valid declaration-keyword
recovery (`fu` to `func`). The preserved `keyword-probe.log` compares that failure
with the original working G76 binary. Provenance is now captured after the actual
successful source/discovery load, before parse/entry/semantic failures can exit;
incomplete graph loads remain unverifiable. This preserves compiler keyword
recovery without pretending that a lexical-only context check validates an
incomplete dependency graph. The registered IntelliSense test now exercises this
transition, a legitimate new unsaved import, and restoration to the prior context.


### Final source qualification and latency

All three profiles were rebuilt/relinked with LLVM/Clang/LLD 23.1.4. Focused
checks ran before their integrated selections. No unrelated full compiler-suite
reruns were required for this editor-only correction.

| Final-source gate | Result |
| --- | --- |
| Debug relevant LSP/tooling/project/session/depth + CLI presentation | **54/54 PASS** |
| Release same selection | **54/54 PASS** |
| Fatal ASan/UBSan/leaks, existing strict registrations | **51/51 PASS** |
| Focused IntelliSense, ownership, recovery, documentation, named arguments, CLI presentation | **6/6 PASS per profile** |
| Actual Debug publication mutation/retry cases | **8/8 PASS**, profiling disabled and enabled |
| Exact-byte native tampering and recovery | **5/5 PASS per profile** |
| Observed-running request cancellation | **8/8 PASS per profile** |
| Isolated real VS Code final runs | **Both PASS**, plus before-reuse comparison PASS |

The extra integrated test relative to the earlier 53/53/50 selection is
`cli_presentation`, included because the observation sink gained editor details.
It checks actual timing JSON against the versioned schema and semantic timing
contracts. The original **1,048,576-byte** frontend-depth test passed in every
profile. Strict retains its three existing LSP stack exclusions and the unchanged
fatal ASan/UBSan/leak options shown earlier. The updated registered IntelliSense
test includes hard-linked package manifests, `fu` recovery, valid new imports,
UTF-16 edits, stale document versions and fresh retries. The debugger harness's
last change only saves stderr; its eight cases were rerun with profiling enabled.

This comparison uses the **fully repaired correctness checkpoint before the
metadata-reuse optimization**, not the pre-G76 compiler or pre-P1 G76 candidate.
Only our benchmark workload ran during these sequential measurements; unrelated
user applications were not stopped or controlled. Profiling is **disabled** in
the following table. Namespace cases use 100 samples after five warmups; local
member/signature cases use 20 samples after three warmups. All samples survive.

| Protocol case | Before reuse median / p95 / worst ms | Final median / p95 / worst ms |
| --- | ---: | ---: |
| Warm rl. existing call | 20.80 / 25.79 / 32.84 | 20.81 / 21.70 / 22.49 |
| Warm rl. newline | 20.67 / 21.88 / 26.21 | 20.94 / 22.58 / 36.78 |
| Warm rl. EOF | 20.76 / 23.83 / 30.13 | 20.94 / 22.63 / 25.65 |
| Immediate edited rl. EOF | 17.91 / 41.23 / 47.27 | 18.38 / 41.89 / 69.86 |
| Warm value-member completion | 14.70 / 15.07 / 15.17 | 15.73 / 16.40 / 16.58 |
| New edited value-member completion | 203.55 / 205.37 / 206.57 | 173.03 / 183.71 / 208.97 |
| Warm signature help | 15.63 / 15.88 / 15.99 | 15.09 / 16.03 / 16.12 |
| New edited signature help | 208.44 / 219.64 / 228.03 | 177.77 / 188.67 / 188.71 |

The first assistance request after an invalid project has **no successful
historical semantic basis**: 176.52 ms before reuse,
156.80 ms after. These are individual initial observations,
not warm medians. Both return the correct receiver members from the valid unsaved
revision. Newly edited member/signature medians improve approximately **15%**.
Their full semantic context still costs more than a cached namespace query;
uncached editing is not claimed to be sub-50-ms. Warm local assistance is below
20 ms; the complete rl. list remains about 20.8 ms. No further fraction-of-a-
millisecond optimization is justified by this evidence.

Opt-in profiling independently confirms the intended effect. Worker dependency
verification median changes from **70.30 to 48.96 ms**, project capture from
**76.08 to 54.82 ms**, and worker total from **177.05 to 147.08 ms**. Lex/parse is
**30.33 / 29.61 ms**, semantic analysis **46.72 / 45.15 ms**, candidate generation
**12.37 / 12.50 ms**, compiler formatting approximately **0.002 / 0.002 ms**.
Final input freshness is **15.51 ms**, response formatting **0.026 ms**, stream
publication **0.010 ms**. Warm namespace freshness is **15.36 ms**, dependency
verification **12.25 ms**, and publication **0.242 ms**. These are inclusive
observations, not exclusive costs or a reconstructed sum. Stage summaries use
only observations that entered that stage; raw rows and counts are retained.

The deterministic changed-dependency cases use a smaller non-native project:
the actual post-resume freshness scopes reject mutations in roughly
**0.91–1.10 ms**, with correct fresh retries. A still-invalid hard-link graph
has no admissible proof and is rejected immediately. Debugger-held publication
wall intervals include the breakpoint pause and are **not** used as latency
benchmarks. The native raylib measurements and the no-history initial request
above separately quantify their substantially larger admitted input context.

Actual VS Code 1.144.0, same isolated harness and sample counts:

| Editor case | Before reuse median / p95 / worst ms | First final run | Final repeat |
| --- | ---: | ---: | ---: |
| Warm provider | 38.40 / 42.75 / 48.75 | 41.29 / 54.44 / 59.79 | 38.91 / 46.37 / 62.69 |
| Rapid rl. popup DOM | 186.60 / 218.50 / 218.50 | 196.80 / 212.10 / 212.10 | 184.95 / 222.20 / 222.20 |
| Rapid rl. two-frame paint proxy | 211.70 / 237.40 / 237.40 | 217.50 / 239.30 / 239.30 | 212.70 / 239.90 / 239.90 |
| Assignment edit through provider | 90.46 / 107.95 / 107.95 | 97.23 / 111.98 / 111.98 | 93.22 / 109.92 / 109.92 |

The initial final provider p95 **54.44 ms** exceeded the 50 ms goal, so one
same-harness before/repeat comparison was justified. It did not reproduce that
elevated p95: before **42.75 ms**, final repeat **46.37 ms**. All three runs and
both final worst cases (**59.79 / 62.69 ms**) remain recorded. The original
**100.66 ms** outlier is also retained. There is no universal sub-50-ms editor
latency claim or repeatable substantial warm-server regression established here.
The UI measurements include extension/client conversion and renderer scheduling;
a two-frame paint proxy is not a hardware display timestamp.

Every editor run verifies real raylib completion, parameter label offsets,
invalid-to-valid diagnostics, hover and semantic tokens; rendered sampling checks
that an unchanged call retains semantic coloring during rapid typing. The final
repeat screenshot was also inspected at
`/tmp/void-g76-freshness-j_ii4ckz/roslyn-editor-final-repeat/workbench.png`.
These are representative isolated Linux observations, not exhaustive typing,
large-workspace memory or native Windows/macOS qualification.

### Final recommendation and evidence

**INTEGRATE recommendation for the G76 correction candidate; ready for the
requested focused Golden Wrench re-review.** No known correctness failure remains
in the relevant final gates. This implementation recommendation does not claim
independent acceptance, permission to commit/push or native platform acceptance.
Windows and macOS remain **UNQUALIFIED**. Native catalog tests do not execute
native libraries or grant execution/install authorization.

The exact **31-path** complete candidate inventory is above; the correction and
Roslyn review touch **13 paths** within it. Source/binary hashes, all final test
names, measured samples, upstream links, debugger results, failed intermediates
and the earlier checkpoint are in `evidence/g76/freshness-results.json`; selected
raw logs are in `evidence/g76/freshness-verification.txt`. Raw fixtures, preserved
binaries, profiles and editor evidence remain in `/tmp/void-g76-freshness-j_ii4ckz`.
All **21** original independent-review evidence checksums still match.

`git diff --check` and whitespace checks for new text files pass. No changes are
staged; no commit, push, installation, publication, SDK change, hosted CI or
worktree operation was performed. G69 remains on its existing HOLD.

Final source navigation (verified line numbers at handoff):

| Change | Source |
| --- | --- |
| Owned assistance input proof and public verifier | `include/void/frontend.hpp:257`, `src/frontend/compiler.cpp:5997`, `src/frontend/compiler.cpp:8251` |
| Project-owned proof, identity readmission and early recovery provenance | `src/frontend/project_selection.inc:363`, `src/frontend/project_selection.inc:511`, `src/frontend/project_selection.inc:596` |
| Unconditional async publication gate and refresh | `src/lsp/main.cpp:1083` |
| Same-job byte-verified dependency reuse | `src/frontend/compiler.cpp:8228`, `src/frontend/project_selection.inc:586` |
| Inclusive opt-in detail scopes | `include/void/compiler_observation.hpp:59` |
| Portable identity/byte regression and package hard-link admission | `tests/tooling/tests.cpp:246`, `tests/lsp/intellisense.py:182` |

Final integrated command, using the existing fatal environment for strict:

```sh
ctest --test-dir build/llvm23-PROFILE --output-on-failure \
  -R 'lsp|tooling|project_protocol|project_session|^text_encoding_semantic_depth$|^cli_presentation$'
```

Reproduce value-member/signature measurements (fresh output required):

```sh
python3 tools/bench/intellisense_assistance.py \
  --server build/llvm23-release/voidls \
  --project /tmp/void-g76-56i5xfx0/raylib-project --samples 20 \
  --output /tmp/fresh-g76-assistance-measurement
```

Run separately with `VOID_LSP_PROFILE=1` for stage observations. Do not compare
instrumented times directly to ordinary measurements or benchmark during builds.
