# Stage 9 independent gap audit

> Superseded CLI/authorization status, 21 October 2026: Boss authorized all three
> Stage 9 milestones and removed `--app` entirely, including compatibility use.
> [Current workflow](STAGE9_APPLICATION_WORKFLOW.md) and
> [acceptance](stage9/ACCEPTANCE.md) supersede conflicting proposals/status below.
> Historical decisions, experiments and verification records remain intact.


21 October 2026. **Research complete; engineering is not authorized.**
Companion: [Stage 9 roadmap](STAGE9_ROADMAP.md). This supplements the historical
[pre-Stage-8 audit](LANGUAGE_COMPLETENESS_AUDIT.md), rather than replacing its
dated experiments or treating their failures as current defects.

## Baseline, authority and verification

Inspected HEAD: `10f77fac23273c5af3fa3be6683c72c8ac059584`, project-local library
foundation. At research start, `.github/workflows/ci.yml`, `docs/README.md` and
`docs/WRENCH_CLANKER.md` already had modifications; `docs/ci87-extension/` was
untracked. Those are existing work, not Stage 9 changes. The workflow repair and
older evidence are not being reimplemented or certified by this research.

Boss's current message authorizes Stage 9 **research only**, superseding older
handoffs saying no Stage 9 work is authorized. It does not declare Stage 8 hosted
locked or authorize production changes. Stage 8's recorded 156-suite consolidated
acceptance and the later local-library focused acceptance concern different
snapshots. The current configured inventory has **157 suites**. Do not multiply
that inventory into a new hosted or full-local acceptance total.

Read the documentation index, Research/Wrench handoffs, approval ledger, current
strategy, Bible including its entry/operator additions, complete known issues,
Stage 8 contracts and current source. Current approved contracts override older
phase exclusions; illustrative Bible examples do not establish implementation.
In particular, Boss approved explicit returns on every path of typed main:
the old research suggestion of implicit zero for typed main was rejected.

Verification in this assignment:

- Rebuilt `voidc`, `voidls`, `void_tests` successfully in `build/ci-release`.
- **16/16** focused Release suites passed, zero failures, **43.57 seconds**.
- **28 source probes:** 13 successful cases also ran in normal/O2 JIT; 15 expected
  rejection cases retained diagnostics. Ten additional CLI cases matched current
  dispatch/usage behavior: **64 compiler invocations** in total.
- **Six** normal/O2 AOT cases emitted, linked and executed: scalar application,
  managed/list application and legacy numeric entry. These are 18 process steps,
  not six new CTest suites.
- Eleven external-linker experiments isolated path behavior, including successful
  controls and a reproduced LLD output-path failure described below.

An initial attempt using the old `build/release` configuration rebuilt voidc but
failed resolving the later voidls target in that invocation. It is not counted as
a successful three-target build. The existing current `build/ci-release` build
then completed all requested targets. No production correction was needed.
No fresh full Debug/Release/sanitizer matrix, 1 MiB gate, interactive editor run or
hosted workflow was performed. Existing acceptance records remain separate.

Reproduction commands:

```sh
cmake --build build/ci-release --target voidc voidls void_tests -j2
ctest --test-dir build/ci-release -j2 --output-on-failure -R '^(frontend|modules|tooling|analysis|sessions|numeric_sprint2|text_collections_sprint3|lsp_stage8|lsp_sprint6|local_libraries|application_native_sprint4|application_native_sprint4_host|ergonomics_sprint6|async_retention_sprint5|source_cli|debug_cache_processes)$'
```

Temporary scripts, logs and complete probe results: `/tmp/void-stage9-research/`.
This document retains the important cases so those temporary files are not a
dependency for implementation. Measured executable SHA-256 values:

| Binary | SHA-256 |
| --- | --- |
| `build/ci-release/voidc` | `e9de90f13cf76a6c8d7a6f107cf473369cebfb917cd0c04b0ced0f2c669acb37` |
| `build/ci-release/voidls` | `757050f2399addd188b4e3858dd9ee2705e5a177d1584c436c8aa53987d137d2` |

## What changed since the previous audit

| Area | Verified current implementation | Remaining boundary |
| --- | --- | --- |
| Calls | Ordinary positional-prefix/named/default mapping, concrete-default annotation inference, generic specialization and source-order evaluation. | Function values retain fully supplied positional signatures; no overload sets/runtime defaults. See [calls contract](STAGE8_CALLS_NUMERIC.md), compiler `argument_mapping` and generic inference. |
| Numeric API | Exact min/max, canonical type/instance abs/clamp, chaining, signed-min checked failure and approved NaN/zero/bounds policy. | Exactly four compiler-owned members; arbitrary primitive receivers/type extensions remain unsupported. `src/numeric.cpp` owns shared semantics. |
| Strings | Strict immutable UTF-8, length/byte_length, negative scalar indexing, owned byte conversion and result-based decode. | Nullable strings, slicing/iteration/ordering, general display/formatting and triple strings remain absent. See [text contract](STAGE8_TEXT_COLLECTIONS.md). |
| Recoverable collections | Fixed/list get, list pop and dictionary get return the approved string-error results while preserving copies, identity and readonly. | No new keys, views, custom hashing/equality, general value removal or implicit nullable collection family. |
| Selected ergonomics | Type-operand has, value-producing indented if, delimiter-aware multiline calls and string-only interpolation. | Both generic branches are checked; has is not conditional compilation. Numeric/bool/custom interpolation is absent. See [ergonomics](STAGE8_ERGONOMICS.md). |
| Applications | Selected-file top-level code or synchronous no-argument main, application status, no-result default zero, typed-main return checking, imported-body rejection. | Mandatory CLI --app remains; general argv/async entry and native build command are absent. See detailed dispatch audit below. |
| Modules | Quoted importer/root-relative VFS; unquoted nested standard sources in isolated domain; compiler-owned discovery/docs/navigation and bounded completions. | No shipped standard catalog: `stdlib/` contains only README. Matching source-root deployment still needs configuration. |
| Local libraries | `lib/name` resolves `/libs/name/name.void`; deeper paths name exact files. Quoted/local paths share project ownership and identity. | No external package boundary, downloader, automatic linking or native authority. See [local library contract](LOCAL_LIBRARIES.md). |
| Project selection/editors | Shared discover_project and voidls preflight, exact main.void discovery, explicit override and fixed-session semantics. | Entry-reachable analysis, one selected project, portable module filenames; not a multi-root project manager. |
| Static providers | Fixture plus independently admitted sink, same ownership/callback/Runtime transport. | Catalog selection remains closed; one admitted descriptor per graph. A second provider proves reuse, not general-purpose FFI. |
| Async retention | Completed execution-frame roots clear after native return/acquired worker completion. | Task/result/Work/group records, null Handle registrations and capacities still grow within a root. No constant-memory claim. |

Independent positive probes returned the expected values in both JIT modes:
ordinary named/default call 8; numeric chain 4; type-style abs 5; i32 max
2147483647; UTF-8 counts for `A🎮B` 9; scalar negative indexing 1; encode/decode
scalar count 3; get/pop 9 each; if-expression 7; multiline call 5; string
interpolation 7; direct using relationship 1.

Independent negative checks:

| Missing/restricted form | Current codes | Assessment |
| --- | --- | --- |
| Numeric interpolation `"{input}"` | VOID2001 | Confirmed incomplete approved display direction; string-only subset works. |
| Source `print(input)` | VOID2003 | Confirmed missing source API, not evidence CLI result printing is broken. |
| Declared private/const fields | VOID1102 | Confirmed missing storage encapsulation; const views and method privacy are separate. |
| Static field | VOID1101 | Accepted broad direction, detailed initialization/thread policy still open. |
| Multiple returns | VOID1101 | Approved feature not implemented; value structs/results are available alternatives. |
| Nullable string/scalar/list | VOID1103 | Nullable spelling is not supported universally; managed/interface/callable nullable paths do work. |
| Generic enum | VOID1101 | Missing extension; concrete tagged enums/results already work. |
| Exponent literal | VOID1004 | Numeric syntax extension absent; core arithmetic is implemented. |
| Omitted slice endpoint | VOID1101 | Current explicit copying slice contract remains intentional. |
| Enum dictionary key | VOID1103 | Enum support does not imply hashing admission. |
| Arbitrary primitive receiver declaration | VOID2018 | Intentional prelude-only boundary, not regression of numeric methods. |
| Typed main falling through | VOID2006 | Correct current owner-approved rejection, not a defect to repair. |

## Application CLI: the outstanding redesign is mostly dispatch

### Actual source boundaries

[src/main.cpp](../src/main.cpp) strips a trailing suffix of --app/--cache/--no-opt,
sets a separate CLI application bool, accepts check/run/emit shapes and compares
that bool with the compiler result. It rejects a valid application without --app
using “Application entries require explicit --app.” Flags are not a general
order-independent parser. No build branch exists; no-path run prints usage.

The frontend already owns entry classification:

- `application_entry` in [compiler.cpp](../src/frontend/compiler.cpp) selects a
  synthetic top-level body first. Without one, the presence of a declaration named
  entry preserves legacy selection; otherwise eligible synchronous main is used.
  An invalid legacy entry is not silently replaced by main.
- `validate_graph` rejects imported executable bodies and top-level/eligible-main
  competition. It does not execute source during checking/editor analysis.
- ValidatedAnalysis retains `application()` and `entry_symbol()`; lower_graph
  constructs the existing private application adapter. No caller should infer
  application mode from filename, the absence of a numeric operand or a second
  parse of main syntax.
- Source compatibility and Debug cache already include application/entry identity;
  JIT/AOT share the same i32(i32) internal adapter and precise Runtime paths.

The simplest next design removes the CLI's mandatory mode mismatch and consumes
this compiler-owned classification in ordinary, cached and emit/build paths.
Keep require_source_entry: declaration-only libraries may check but must not run
arbitrary exports. Removing --app must not remove signature/body validation.

### Observed dispatch matrix

| Invocation/source | Current result | Stage 9 recommendation |
| --- | --- | --- |
| `run main.void`, eligible main returning 7 | Failure 1, requires --app | Run classified application; status 7, empty stdout. |
| Same with --app | Status 7, empty stdout | Preserve as optional compatibility spelling initially. |
| `run <directory>` with application main.void | Failure 1 without flag; status 7 with it | Same automatic classification after existing discovery. |
| `run tool.void --app`, top-level return 3 | Status 3 | Explicit application files already work; make flag optional, keep root = selected file parent. |
| `run` in a project with main.void | Usage 2 | Recommend cwd main.void discovery; mark this as new CLI behavior. |
| `build main.void` | Usage 2 | Implement native executable production using existing AOT/link path. |
| Application plus numeric input | Usage 2 | Keep rejecting unsupported arguments; do not reinterpret them as legacy input. |
| Declaration-only legacy entry plus ordinary main | Legacy output `7\n`, status 0 for input 5 | Preserve precedence, numeric input and print/status behavior exactly. |
| Legacy source forced with --app | Failure 1 | If --app is retained as an assertion, keep a clear wrong-mode diagnostic. |

Two kinds of zero must stay distinct. A no-result application or top-level
fallthrough maps to process status zero. A successful legacy invocation prints
its result and returns process status zero even when its result is 7. Explicit
typed main must still return on every path. Do not revive the earlier unapproved
implicit typed-main zero recommendation. Imported main is never an entry candidate.

The dispatch policy, --app compatibility period and default no-path run behavior
need a dated approval superseding Sprint 4's explicit-mode contract. Optional-main,
coexistence and return-type policies are already approved and need no redesign.

## Native executable build: reusable path and real dependencies

### What exists

`emit_object` in [backend.cpp](../src/backend/llvm/backend.cpp) already emits a
host-native object with the source main adapter. Application objects return status;
legacy objects parse optional i32 input, print the value and return zero. Ordinary
managed source objects call the Runtime ABI. `emit_runtime_object` is a different
hosted/proof path and should not replace the normal source emitter.

The shared linked-source tests show the actual complete link. Scalar-only tests
can use a C driver and libc, but managed/text/collection programs require the C++
Runtime archive and its existing native fixture/sink archive plus thread support.
[sprint4.py](../tests/stage8/sprint4.py) and
[sprint6.py](../tests/stage8/sprint6.py) use:

```text
clang-19 --driver-mode=g++ [configured linker flags] program.o
    libvoid_runtime.a libvoid_native_fixture.a -pthread -o program
```

This is a Linux illustration, not a universal command line. Existing Windows tests
also select the dynamic MSVC runtime and legacy_stdio_definitions. macOS needs the
configured SDK/driver environment. Keep archive order and compiler/runtime ABI,
configuration and sanitizer linkage matched. The generated executable needs VOID's
Runtime support, not LLVM compiler libraries merely because voidc uses LLVM.

The official [Clang 19 toolchain guide](https://releases.llvm.org/19.1.0/tools/clang/docs/Toolchain.html)
explains driver-selected startup/runtime dependencies and the C++ linking mode.
It is a 19.1-series reference; production stays pinned to 19.1.7. Current test
commands and successful local links provide the project-specific evidence.

The six fresh AOT checks linked scalar app, list-backed app and legacy source
through the current Runtime/native archives in normal/O2 modes. Apps exited 7
without printing; legacy input 5 printed 7 and exited zero. This demonstrates
reuse, not implementation of build or clean-machine portability.

### What build must add

1. Shared CLI selection/analysis/entry validation, then the existing object emitter.
2. A small host link configuration from the selected matching toolchain: driver,
   configured linker, correct Runtime/support archives and required platform flags.
3. Safe argv process execution, source-facing failure messages with retained driver
   detail, temporary object/executable handling and success-only final publication.
4. Deterministic build-tree and private install resource lookup. CMake currently
   installs voidls/adapters, **not voidc plus a usable Runtime link installation**.
   An installed server bundle is not a compiler SDK. Runtime archive location and
   ABI matching must be established before claiming installed build works.

The existing CMake VOID_CLANG lookup/version check is inside BUILD_TESTING and
does not configure a production CLI linker. Reuse its pinned-driver policy while
making necessary host-link inputs available in non-testing compiler builds too.

Reuse CMake's existing target/toolchain knowledge; emit only necessary host-link
configuration rather than a general build graph/package manifest. No compiling
Runtime C++ source on every application build, LLVM compiler library linkage into
every application, source-to-C backend, download manager or implicit native
provider authorization is needed. Library imports never grant linker inputs or
host binding permissions. Building native-backed wrappers remains limited to
explicitly admitted hosts/providers.

### Reproduced LLD output-path issue

With the installed pinned Clang/LLD 19.1.7, a trivial C program linked to an
**absolute executable path containing `%` in a parent** failed with “cannot open
output file ... No such file or directory,” although that directory existed.
The same program/output linked with the default system linker. LLD controls using
plain, space-containing and Unicode paths succeeded. This independently confirms
the observation in [local-library acceptance](local-libraries/ACCEPTANCE.md);
it is not a VOID VFS defect or justification to reject valid project paths.

Further experiments linked successfully with relative output names from the
percent-containing directory, including a percent-containing filename. Passing
Clang `-working-directory <that directory> -o <relative-name>` also succeeded,
without changing the parent process cwd. This is a demonstrated candidate for
staging/linking, not a universal workaround certification for every driver/OS.
Keep source/object paths and diagnostics intact; validate on native platforms.
Do not hide the issue by making the eventual build test choose an unrelated clean
output directory. Final publication should preserve a previous good executable
on compile/link failure and respect platform replacement constraints.

LLVM 19.1.7's installed `llvm/Support/Program.h` provides argv-based process
execution and distinguishes launch failure, child status and crash/timeout.
It does not provide a child-cwd parameter in ExecuteAndWait. The measured Clang
working-directory option can avoid an unjustified global cwd change. Engineering
may choose another small existing process facility if necessary, but must not
construct shell command strings from source/output paths.

## Known issues reconciled against current source

Disposition labels: **fixed/superseded**, **partial**, **open**, **intentional**,
**unverified**. “Open feature” is not automatically a safety defect or Stage 9 scope.
This table consolidates every substantive family in the complete known-issues
file; dated records and detailed earlier audit remain available as history.

| Issue family / originating records | Current disposition and evidence | Stage 9 consequence |
| --- | --- | --- |
| Stage 6/7 pending locks and no Stage 9 research authorization | Superseded by later owner evidence/current research message. Stage 8 lock still unclaimed. | Preserve historical acceptance; no self-declared hosted progression. |
| Stage 8 known-issue blanket statements about missing indexing/get/pop/interpolation/async retirement | Partially superseded by Sprints 3/5/6. Contracted subsets verified; broader formatting/record retirement remain absent. | Replace blanket conclusions with exact subset statements in handoff. |
| Deep valid receiver-chain small-stack crash | Fixed in Stage 7; numeric/text additions had separately repaired frame regressions in Stage 8 evidence. | Preserve 1 MiB gates on eventual changed compiler code; no new smaller semantic budget. |
| Malformed UTF-8 diagnostic output/caret issue | Fixed by escaped-byte renderer and immutable snapshot regressions. | Preserve strict UTF-8/UTF-16 mappings and Windows wide-argument boundary. |
| Large arrays/64 KiB production restriction | Superseded; production arrays, compact shape analysis and large snapshots work. Research-only storage-audit bound is not a language cap. | No new memory ceiling or alternate memory manager. |
| Layout/depth/expanded-field/generic/interface/construction work guards | Intentional resource boundaries, distinct from byte-size caps. | Preserve rejection/recovery unless a concrete supported workload proves a defect. |
| Ordinary named/default/inferred parameters missing | Fixed within approved pure-default/ordinary-call subset. Callable/event/interface restrictions remain. | Do not schedule another binder implementation. |
| Numeric primitive properties/methods missing | Fixed for the four-member prelude; arbitrary extensions/operator overloading absent. | Keep one canonical implementation and current numeric edge rules. |
| String scalar access/UTF-8 conversion absent | Fixed within Sprint 3; nullable/slicing/ordering/formatting/triple strings still open. | Library/display backlog, not a reason to duplicate UTF-8 storage. |
| String concat scratch/quadratic growth/root retention | Open performance characteristics, no new fault reproduced. Existing linear traversal measurements support no index cache yet. | Measure representative formatting workloads before caches/builders. |
| Printing and numeric/bool/custom interpolation | Open approved direction; print is unknown and numeric interpolation rejects. | High-value next library/display contract; not silently included in CLI work. |
| Dictionary/list recoverable get/pop | Fixed approved result<string-error> APIs; builtin method values and synthetic worker-call admission still restricted. | Preserve worker effect rejections, not accidental new permissions. |
| Dictionary unsupported keys, custom hash/equality, randomized/stable hashes | Intentional/deferred; enum keys still reject. Pure templates work, queries remain runtime. No clone/view promised. | Do not let future custom equality change key hashing incidentally. |
| Slices/views/value remove/custom iterators | Explicit copying slices implemented; omitted endpoints/views/search and iterator protocol open. Reverse-remove exception deferred. | Existing next-access iterator invalidation remains authoritative. |
| Generic enums/unit payload syntax/multiple returns | Open completeness; concrete results/enums/value structs provide alternatives. | Not needed to wrap existing entry ABI or link applications. |
| Primitive/collection/string nullable forms and as? | Partial nullable family; managed/interface/callable forms/patterns work. Nullable conversion expansions need separate rules. | Do not label missing syntax as a current authority escape. |
| Private/const fields/static state/runtime field defaults | Open completeness/design. Struct storage fields have no visibility/readonly declaration metadata; const paths work. | Real encapsulation gap for wrapper libraries; scope separately from linker plumbing. |
| C1/S1 construction restrictions, helper first writes/self escape | Intentional safety contract; complete-value prerequisites and staged publication remain. | No entry/build refactor may bypass constructor validation. |
| Fallible/overloaded init/value-resource deinit/foreign extensions | Deferred; result-returning factories and managed deterministic cleanup work. | Do not invent rollback or copy ownership as collateral work. |
| Composition/inheritance advanced features | One-hop stored promotion/single managed inheritance work. Transitive/stateless/protected/hiding/covariance/abstract/final/open-world remain deferred. | Keep owner/import rules and no implicit component conversion. |
| Interface “no type tests,” boxing and richer interfaces | Guarded managed/interface patterns supersede blanket no-tests wording. Standalone boxing/inheritance/defaults/Self/generic interfaces/general casts remain unsupported. | Preserve data-only witnesses and generation-owned dispatch. |
| Conservative readonly callbacks/effects/families/joins | Open restriction; unrelated providers may cause conservative rejection. No unsound escape reproduced. | No path-sensitive alias framework justified by CLI goals. |
| Precise GC, automatic collection and async roots “absent” | Superseded: coordinated roots/safepoints/allocation budgets and async roots implemented. | Preserve GC/failure cleanup, not manual source GC requirements. |
| Async execution-frame retention | Partial, materially improved: frame roots clear after safe quiescence; results/native records and root slots retained. Runtime source still linearly scans retained tasks. | Do not repeat Sprint 5 or delete records without a new ownership proof. |
| Long-lived Task/result/Work/group metadata/capacity growth | Open resource limitation, documented measurements. Zero live payload is not constant RSS. | Separate measured follow-up if applications demonstrate need. |
| Dropped native owners retained until Runtime shutdown | Intentional; GC does not call deinit. Resource transfer/callable/native worker privileges remain bounded. | Keep explicit shutdown/one-shot cleanup in built apps. |
| Static provider “fixture only” | Partial: independent sink exists; catalog remains closed, one provider descriptor per graph. | No general provider/FFI/package authorization from local source imports. |
| General dynamic libraries/UTF-16/native aggregates/callback authoring | Deferred. Owned/retained/returned UTF-8 fixture/sink paths work; NUL policy belongs to each bridge. | Separate from host executable link dependencies. |
| Arbitrary argv/async CLI/general entry ABI | Open. New synchronous applications and explicit file selection already work under --app. | Preserve source entry semantics; Stage 9 removes mode friction, not adds every entry ABI. |
| Executable build/link orchestration | Confirmed absent; existing native object emission/manual test links work. | Core Stage 9 delivery. |
| AOT stdout versus JIT stderr checked diagnostics | Open presentation inconsistency explicitly retained by Sprint 4. Runtime source sites/traces work. | Include bounded channel consistency and failure/status tests. |
| Warnings/recovery/debug details | Warnings implemented; lexical cascades, bounded diagnostic/trace budgets, opaque managed debug metadata, richer native/foreign stacks and backend error translation remain. | Improve changed CLI boundaries; no generic diagnostic framework. |
| Local/standard imports/discovery/docs “absent” | Superseded by Stage 8 and local libraries. External identities/permissions/package resolution remain absent; stdlib catalog is empty. | Reuse current SourceGraph/metadata and explicit matching stdlib-root policy. |
| Unicode source versus module filenames/aliases | Strict UTF-8 source and Unicode physical roots implemented; portable ASCII module components, exact case and alias rejections retained. Discovery received targeted platform repairs. | Native Windows/macOS verification remains distinct from source inspection/Linux success. |
| Runtime module globals and imported scene instances | Open/undecided; top-level app locals do not implement imported mutable globals. | No import-time execution introduced by easier CLI. |
| LSP/hover/completion/rename/formatter/watch/save/refresh absence | Superseded within existing subset. Entry-reachable full analysis, strict UTF-16/local URIs and bounded recovery remain. | Do not reopen Stage 7 as a new language server. |
| Completion crash on incomplete u / $application metadata | Fixed current candidate-insertion invariant and regression; no catch-all suppression. | Retain same-process incomplete typing/repair checks. |
| Rename types/families/captures and Vim version enforcement | Intentional exclusions; Vim rename disabled safely. Guarded ordinary project edits work. | Build/run changes grant no new edit authority. |
| Semantic tokens/general quick fixes/workspace expansion | Deferred. Unreachable overlays are not independently semantically validated; synchronization is model-lifetime based. | Path listing does not mean whole-workspace semantic compilation. |
| Disk non-atomicity/history growth and scheduling | Snapshot/token stale rejection and refresh implemented; no atomic filesystem snapshot/total project quota. Queued batching works; active compile not interruptible. | Reuse freshness; no worker pool/incremental compiler requirement. |
| Optional setTrace unsupported | Fixed notification handler; trace may contain source. | Preserve lifecycle and source logging expectations. |
| Cache Windows/unidentified compiler bypass, corrupt misses, eviction | Safe deliberate fallback plus deferred maintenance. Whole-graph cache works where supported. | Do not turn build into a second persistent cache system. |
| Reload suspended-task tracking/constant dependency history | Bounded task leases and exact frontend contracts supersede older absences; const/layout/capture changes still restart. | Auto CLI classification must preserve entry-kind contracts. |
| Automatic source publication/multi-slot transaction/migration | Deferred; editor refresh is not runtime reload. Lease/dispatch overhead not newly benchmarked. | No implicit --live/watch host in Stage 9. |
| LLVM SDK checksum/signature | Open supply-chain work; downloader has no independent archive integrity check. | Separate focused maintenance, no version upgrade. |
| RTTI/macOS DataLayout/Windows SDK ABI incidents | Specific earlier repairs and later results supersede old failures. Native sanitizer ABI/coverage constraints remain. | Keep CMake ABI/runtime policies in new link config. |
| Historical LLD scheduler trace with zero exit | Unverified root cause; retain if it recurs. Distinct from the freshly reproduced percent-output issue. | Do not claim a historical causal fix based on green exit alone. |
| LLVM header warning/optional CURL/ptrace LeakSanitizer | External warning/optional dependency and execution-context boundary; no basis to suppress project/leak checks. | Record environmental failures accurately. |
| No-RTTI vptr/JIT instruction sanitizer limits/interior pointers | Intentional supported coverage/model boundaries. C++ instrumentation does not sanitize every LLVM-generated instruction. Corrupt roots fatal; uncooperative Managed threads block sweep. | Preserve observable root checks and supported sanitizer matrix. |
| CI #87 extension path and native client/clean-machine claims | Existing reported targeted repair, not a fresh hosted result. Unexecuted VSCodium/editor versions/platforms remain unverified. | Preserve dirty repair; plan actual native acceptance, not another repair sprint without failure evidence. |

No new managed-memory safety or arithmetic-correctness defect was reproduced in
this bounded investigation. That is not a proof of their absence. Confirmed
current usability gaps are mandatory --app, missing build and inconsistent AOT
diagnostic channel; the external LLD percent-path failure is independently
reproduced. Many remaining language items are approved-but-incomplete or open
extensions, rather than regressions.

## Scope judgment and handoff

Recommend Stage 9 finish the application workflow: automatic compiler-derived
CLI entry selection, native executable build with matching Runtime/toolchain,
reliable artifacts/failure diagnostics and shared editor/CLI regression acceptance.
This is useful without inventing more language syntax.

The next language usability candidates are primitive display/print, field
encapsulation and narrowly defined operator overloading. Operator direction is
approved in Bible §66.28, **not implemented**: exact grammar/operands/equality/hash
rules remain open and require explicit stage scope. Do not accidentally merge
these with a CLI/linker milestone. General statics, multiple returns, nullable
extensions, custom iteration and richer strings can follow real application need.

Keep the future **Git-based package manager** separate. Preserve current `libs/`
source mapping as a consumer boundary; do not invent a repository URL/version/
lockfile protocol, fetch dependencies, run install scripts, resolve native
libraries from imports or treat project-internal visibility as an external package
contract. There is no complete prior package-manager specification in the sources
inspected here; do not fabricate one from illustrative SDL/raylib names.

The roadmap lists decisions, implementation boundaries and acceptance. Research
ends here for Wrench review and Boss authorization. No production source, root
README, workflow, Git history or hosted CI was modified by this mission.
