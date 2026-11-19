# G75 — CLI experience implementation and qualification

18 November 2026. Baseline: `fdcbeadbdd8a0011751b448afa9fe40e5151ab1f`
(G74, checkout HEAD and locally recorded `origin/main`; no fetch).
Boss approved the five stages in the [architecture review](G75_CLI_EXPERIENCE_ARCHITECTURE.md#12-bounded-implementation-stages-after-approval),
including the actual-child F5 success receipt. That approval supersedes the
architecture-only hold. G69 remains on hold; G76 remains queued.

**Status: the three Golden Wrench findings are corrected locally and await
review. Current targeted integration passes 44/44 in each of Debug, Release and
fatal ASan/UBSan/leak configurations; editor units pass 11/11 and nine real
Linux VS Code launch cases pass. Native Windows/macOS remain unqualified.
Boss accepted the previously measured opt-in JSON cost. No feature work resumed.**

The [Golden Wrench correction record](G75_GOLDEN_WRENCH_CORRECTIONS.md) records
the exact corrections, original failures, unsuccessful intermediate evidence,
final checks and performance limits. The earlier complete Linux gates remain
preserved at Debug 516/516, Release 503/503 and strict 493/493. Those full suites
were not rerun for this bounded CLI/editor correction; the appropriate 44-test
integration set and final focused gates were rerun against the changed source.

The [Debug correctness follow-up](G75_DEBUG_CORRECTNESS_REPAIR.md) records the
authorized sentinel fix, original-failure preservation and wider regression
results. The earlier sentinel-only totals were Debug 513/516, Release 502/503 and strict
492/493, with zero remaining skips. Those historical failures are preserved;
the current frontend/onboarding follow-up is recorded separately. The feature qualification and performance
measurements below describe
the original pre-repair candidate; they are retained as historical evidence.

Implementation is local and unstaged. This is Keyboard’s implementation and local
qualification record. Independent Golden Wrench review returned CORRECTION
REQUIRED; the corrections require subsequent review. Hosted acceptance has not
occurred. No workspace commit, push, installation into a user
prefix, extension installation, publication or CI dispatch was performed.
Private VOIDUP installation fixtures and disposable editor development hosts are
qualification workspaces, not distribution of the candidate.

## Implemented stages

| Stage | Result |
| --- | --- |
| 1. Help | Extracted command inventory, usage and help presentation. Top-level and per-command option tables calculate their own widths and use spaces. All fourteen commands, existing aliases and accepted nested help routes remain under their original parsers. Full command pages include defaults, examples, output effects, authorization limits and exit contracts. |
| 2. Wall timing and status | `--status auto\|plain\|off` for source/build operations. Auto requires eligible interactive stderr; redirected stdout and stderr retain their existing defaults. Fixed phase lines, accurate manifest profile labels and compiler totals precede application invocation. No framework, color, spinner, input-mode change or animation thread. |
| 3. Detailed observations | `--timings`, `--timings-format text\|json`, `--timings-output PATH`. Invocation-owned monotonic records cover twenty real categories. Inclusive scopes aggregate without adding nested intervals into a fabricated total. JSON requires an explicitly selected fresh report outside source/project/artifact and protected store trees. Exclusive publication reuses the existing checked project-file primitives. |
| 4. VOIDUP | Small adapter through existing console/time APIs. Global `--status` applies to install/default/rollback/verify/doctor. Fixed labels surround existing calls; trust, transactions, installation, activation, rollback, recovery and forwarded process policy remain authoritative. Run/which/list receive no new status. |
| 5. VS Code | Bounded resource-independent capability negotiation precedes new compiler flags. Older/invalid responses keep the original argv. Managed F5 requires a matching actual-child receipt and a newly published ordinary executable, rechecks identity before launch, and consumes success once. Pending, failed, cancelled, changed and concurrent attempts cannot borrow another receipt. |

The [CLI reference](../CLI.md#status-and-compiler-timings-g75) documents syntax and
stream policy. The [JSON schema](../schemas/cli-timings-v1.schema.json) is version 1
and permits additive fields. Real emitted reports are validated against its
required fields, types, enums, numeric bounds and nested item definitions.
Additional semantic assertions cover unavailable data, parent bounds, cache hits,
application/legacy status and actual publication outcomes.

Observations borrow a thread-local binding whose storage belongs to one CLI
invocation. Library callers default to no sink. Worker threads do not inherit it;
ORC materialization is the caller's combined wall interval. Disabled detail takes
no per-phase clock samples and allocates no per-source record/string. The CLI
still takes outer wall samples. No observer fields enter source, semantic, cache,
native compatibility or Runtime ABI identities. A rebuilt compiler naturally has
its existing whole-compiler build identity, as before G75.

Timing durations are inclusive, rounded once to integer microseconds. Nominal
clock tick size is reported separately from uncalibrated effective resolution.
Lex/parse and ORC internals remain combined. The toolchain/link/publication and
package intervals describe existing operations; LLVM passes and execution order
are unchanged. The authorization callback is observed inside frontend wall time.
No human-wait duration is inferred by subtraction.

Report configuration errors return usage status 2 before compilation. A late
write or exclusive-publication failure is visible on stderr and preserves the
original compiler/application status and any successfully published artifact.
Cosmetic compiler writes handle closed stderr without installing process-global
signal handlers. Fatal termination cannot guarantee a final report or cleanup;
uncertain staging is retained rather than traversed or overwritten.

## Regression qualification

All execution below was native **Linux x64**, LLVM/Clang/LLD **23.1.4**.
Release uses `build/llvm23-release`; Debug uses `build/llvm23-debug`; strict uses
`build/llvm23-strict`, RelWithDebInfo with the existing ASan/UBSan configuration.
Strict runs set `ASAN_OPTIONS=detect_leaks=1:abort_on_error=1` and
`UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1`. SDK RTTI remains disabled as
configured; vptr sanitizer coverage is unavailable in that ABI.

| Evidence | Result and scope |
| --- | --- |
| Fresh G74 baseline | Rebuilt source before behavior probes and retained the compiler/server plus original native benchmark script. Original CLI contract passed. Twenty original native benchmark repetitions are preserved. The review's earlier stale compiler is not used as baseline evidence. |
| Release regression | 23/23 passed: dependency explain, managed native, app/native packaging, managed resolution, legacy entry, templates, maintenance, creation, discovery, profiles, native debug info, protocol, nested CLI, public CLI, presentation, automatic cache, named-argument LSP, OS/log/collections cache, native import and mixed ABI. |
| Strict regression | 12/12 passed under fatal sanitizer/leak settings: dependency explain, managed native, app package, legacy, templates, profiles, native debug info, CLI/presentation, automatic cache, native import and mixed ABI. |
| Final CLI checks | Release: 135 public process checks and 29 presentation process checks plus pipe/PTY/collision controls. Final strict CLI/presentation/profiles: 3/3. Final Debug CLI/presentation: 2/2. These include the completed option tables and actual package/late-report tests. |
| Debug broader regression | **8/11 passed; 3 failed** with an existing backend assertion. This lane is not accepted as an all-pass result; attribution is below. |
| VOIDUP | Final adapter smoke passed. Presentation: 10 captured processes plus six native PTY controls. Signed A/B acceptance: 35 successful/expected-status process checks, including normal/O2 JIT, independent AOT and LSP. All 30 hostile authenticated metadata/package cases were rejected, with TLS/unknown-root refusal. Real 1 MiB partial download, writer-lock contention, SIGKILL, retained abandoned data and authenticated retry passed. Unix smoke uses a real 1 MiB stack. |
| Editor unit tests | 10/10 with the freshly rebuilt `voidls`, including receipt and capability controls. `npm test` now includes both test modules. |
| Real editor overrides | VS Code 1.144.0 with LLDB DAP 0.7.20261117. Slow task's actual ten-second Debug Anyway choice: no adapter while pending; later child failure preserves old executable. Missing-task exception's real Debug Anyway choice: receipt refusal, no child or adapter. Zero-exit compiler without new publication: refusal. Actual rebuilt G74 compiler: no new status flag, successful build and LLDB launch. |
| Ordinary debugger regression | Existing real missing/installed adapter and starter launch suites passed. Starter build statuses `[0,1,0]`, observed application exits `[0,17]`, source breakpoint, step and scalar value 14; failed build retained the old executable. |

Default pipe stream assertions, Unicode/space paths, unavailable resources,
metadata without stdlib/native manifests, `CI`, `TERM=dumb/unknown/unset`, absent terminfo, native PTYs, closed
stderr, failed analysis, cache hit omission, O0 optimization omission, modern
exit 7, legacy result 7/process exit 0, exclusive report collisions, alias refusal,
linked artifact survival and actual package timing JSON are exercised. Existing
cache hashes/timestamps, lock verification, native consent, package checks,
application literal forwarding and exit codes retain their existing assertions.

Windows and macOS are **UNQUALIFIED for G75**. No Linux result qualifies native
Windows console/ConPTY, long paths, cancellation or cppdbg/GDB, or macOS terminal,
JIT/AOT and exclusive publication. Existing platform restrictions and known
Windows debugger displayed-exit limitations remain. This original feature report describes focused regression
coverage. The subsequent repair report records wider compiler suite execution;
neither report qualifies every native editor/raylib lane. Receipt concurrency/cancellation/selection controls include pure
state tests; not every host cancellation race was replayed interactively.

## Preserved failures and attribution

The Debug runs of `project_managed_native`, `project_manifest_legacy` and
`project_mixed_native` all abort with the same
`vector<vd::ir::Instruction>::operator[]` bounds assertion. The legacy/minimal
reproducer was separately backtraced to `block_allocation_elision`, index
`4294967295`. The other two failures were not individually backtraced or replayed
on G74; assigning that exact cause to both remains unverified.
Release and strict pass their selected controls, but that does not erase Debug's
failure. A separately built untouched G74 compiler reproduces the same assertion
and index at baseline `src/backend/llvm/backend.cpp:567` (candidate line 574).
The ordinary G74 legacy test also aborts at the same missing-context application.
Both LLDB backtraces and raw test logs are retained. No optimizer, semantic or
assertion-disabling workaround was introduced. This was the pre-existing compiler
defect that prompted the subsequently authorized [bounded repair](G75_DEBUG_CORRECTNESS_REPAIR.md).
The original failed run remains a failed historical result.

The confirmed source defect is a terminator-use sentinel: the use index records
return/branch escapes as `~0u`, while heap-affinity propagation indexes
`instructions[use.index]` before checking the use kind. The Debug vector assertion
catches the resulting invalid index. The Release and strict passes do not prove
that bookkeeping correct.

That external baseline was extracted from HEAD into disposable storage. Its
unchanged verification tools and active SDK/provider inputs were referenced from
the checkout so absolute-path provider receipts remained valid. Configure-time
verification stayed enabled. Initial external config attempts failed on missing
retained curl patches, missing source archives and lexical SDK path identity;
those logs remain, followed by the verified successful build.

Other unsuccessful evidence remains available:

- An obsolete Slice 1 test expected bare `new` to fail. Fresh G74 actually lists
  templates successfully. The test now checks that established behavior; no
  dispatch/parser change was made to satisfy the obsolete assertion.
- The pre-existing November fixture catalog expired. VOIDUP correctly rejected
  it. A new isolated current test catalog was signed with existing private test
  roots. Production trust/time validation was not relaxed. A first fixture
  symlink failed the test server's containment rules; physical fixture copies
  passed. No private keys are included in retained public evidence.
- Early editor unit runs selected a stale server through the wrong test env var;
  corrected runs use `VOIDLS_TEST_SERVER` and pass. Early exception harnesses
  encountered VS Code's test-mode modal policy, activation deadlock or an
  undismissed final refusal dialog. The final normal-editor harness exercises
  the actual exception UI and dismisses the guard's refusal. Failure logs remain.
- An early editor development harness used the host's default shared task-history
  storage despite an isolated profile. Final override replays explicitly isolate
  `--shared-data-dir` as well. User settings and installed extensions were not
  edited; a task-history entry may remain from that early harness.
- A zero-sized pseudo-terminal did not advertise the existing console dimension
  capability. The maintained VOIDUP PTY test now sets 80x24 dimensions; the
  original failed check is retained.
- Initial C++ integration compile errors and one interrupted benchmark while
  completing the option-table review are retained. No partial benchmark is
  represented as a completed performance result.

## Performance

**Performance acceptance is blocked on a Boss decision for JSON report cost.**
Default redirected and plain/off modes show no repeatable substantial regression
in these controls. Default medians across the full process/request set range
from -5.69% to +4.10% versus G74; the largest increase is metadata startup,
4.894 to 5.094 ms (+0.200 ms). Other candidate modes run that identical startup
command at 4.830–5.056 ms, showing the live-desktop variability. Inventory check
is +0.59%, O2 JIT +0.16%, and repeated O2 build +0.41%.

JSON is a repeatable substantial short-command cost: tiny check is 6.638 to
13.597 ms versus candidate default (+6.959 ms, +104.8%); inventory check is
46.144 to 53.281 ms (+7.137 ms, +15.5%). O2 JIT is +7.829 ms (+4.4%); repeated
O2 build +7.320 ms (+2.2%). These unfavorable results are retained and are
**not accepted without Boss approval**.

A separate interleaved twenty-repetition tiny-check probe measures G74 6.475 ms,
candidate default 6.441 ms, text detail 6.323 ms, JSON under /tmp 13.562 ms,
and JSON under the external XFS archive parent 17.957 ms. The text detail
control does not show a repeatable substantial slowdown here. File mode includes
additional destination admission, serialization, content verification, fsync and
exclusive publication. The exact contribution of each is not independently
measured; no phase attribution is invented. The /tmp ancestor held about 2,272
entries. Parent ancestry and filesystem behavior are part of this opt-in cost.
No case/alias admission or publication protection was relaxed to improve the
numbers.

Before the bounded file-cost probe, online research checked the established
[Linux pathname/dcache behavior](https://www.kernel.org/doc/html/latest/filesystems/path-lookup.html)
and [LLVM JSON API](https://llvm.org/doxygen/JSON_8h.html). Lookup caching is not
an equivalent replacement for case-collision enumeration. The executing pinned
implementation already provides the JSON serializer used here. No applicable
upstream compiler fix was identified or adopted; no LLVM version, pass or
security-policy change is proposed.

VOIDUP doctor, twenty interleaved repetitions each: G74 median 12.150 ms,
candidate default 12.264 ms (+0.114 ms, +0.93%), off 12.213 ms and plain
12.399 ms. G74 p95/worst 13.896/16.957 ms; candidate default 13.498/14.523 ms.
The final rebuilt VOIDUP executable is byte-identical to the binary used in the
signed acceptance/rejection/interruption replay.

All 462 inventory/control timing reports plus 40 file-cost-probe reports
(**502 actual reports**) passed the versioned schema and phase-parent bounds.
The full metric set, per-mode n/median/p95/worst, paired changes and every raw
sample are retained in the external evidence archive. Representative whole-process
and request medians and selected tails follow; LSP repeated requests have
200 observations per method, other shown metrics have twenty.

| Whole-process/request metric | G74 median ms | Default | Off | Plain | JSON detail |
| --- | ---: | ---: | ---: | ---: | ---: |
| `cli.startup` | 4.894 | 5.094 | 4.830 | 4.996 | 5.056 |
| `control.check` | 6.630 | 6.638 | 6.537 | 6.411 | 13.597 |
| `control.check.failure` | 6.139 | 6.066 | 6.223 | 5.992 | 13.594 |
| `control.jit.no_cache` | 15.249 | 15.435 | 15.282 | 15.244 | 22.684 |
| `control.emit` | 15.349 | 15.158 | 15.137 | 15.260 | 22.274 |
| `control.build` | 172.228 | 173.115 | 173.307 | 173.095 | 182.122 |
| `cli.check.repeat` | 45.872 | 46.144 | 45.891 | 46.883 | 53.281 |
| `cli.jit.normal` | 158.976 | 157.549 | 157.093 | 158.626 | 165.158 |
| `cli.jit.o2` | 176.416 | 176.693 | 175.513 | 175.912 | 184.522 |
| `cli.emit.normal` | 391.072 | 380.667 | 389.879 | 391.042 | 395.355 |
| `cli.emit.o2` | 486.853 | 470.435 | 479.404 | 476.590 | 491.084 |
| `cli.build.normal.repeat` | 304.965 | 304.206 | 305.542 | 306.799 | 313.717 |
| `cli.build.o2.repeat` | 326.176 | 327.527 | 325.072 | 329.183 | 334.847 |
| `cache.first` | 160.051 | 158.986 | 157.809 | 163.352 | 166.653 |
| `cache.repeat` | 51.581 | 50.947 | 50.721 | 51.754 | 58.826 |
| `cache.o2_repeat` | 51.104 | 50.520 | 50.838 | 51.128 | 57.360 |
| `cache.source_changed` | 158.951 | 158.412 | 158.908 | 159.298 | 167.607 |
| `cache.manifest_changed` | 158.742 | 157.931 | 160.499 | 160.000 | 167.508 |
| `cache.dependency_changed` | 161.440 | 156.030 | 157.868 | 162.149 | 163.794 |
| `lsp.project_open` | 50.385 | 49.980 | — | — | — |
| `lsp.completion.repeat` | 24.498 | 24.037 | — | — | — |
| `lsp.hover.repeat` | 0.217 | 0.217 | — | — | — |
| `lsp.semantic_tokens.repeat` | 0.200 | 0.197 | — | — | — |

| Tail metric | G74 p95 / worst ms | Default p95 / worst | JSON p95 / worst |
| --- | ---: | ---: | ---: |
| `control.check` | 8.118 / 8.151 | 8.180 / 9.303 | 14.292 / 14.345 |
| `cli.check.repeat` | 49.334 / 50.935 | 49.406 / 53.577 | 57.856 / 59.404 |
| `cli.jit.o2` | 183.876 / 185.870 | 183.406 / 186.167 | 194.262 / 202.568 |
| `cli.build.o2.repeat` | 339.012 / 356.651 | 338.128 / 362.203 | 359.149 / 391.014 |
| `cache.repeat` | 56.603 / 59.753 | 56.695 / 57.836 | 61.908 / 64.719 |


The original 20-repetition G74 native benchmark remains unchanged in evidence.
The final comparison rotates and reverses five modes per cohort: rebuilt G74,
candidate default redirected behavior, explicit status off, status plain, and
JSON detail. Each mode uses the frozen maintained inventory project and fresh
private project/cache copies, twenty repetitions, exact output/exit/cache oracles,
normal/O2 JIT, object emission, independent linking, build, AOT execution and
source/manifest/dependency invalidation. Tiny success/failure checks, uncached
JIT, emit and build are separate controls. Baseline and candidate also run the
same real LSP workload with ten repeated queries per method.

Whole-process observations include startup, status writes, report admission and
serialization/publication. Recorded compiler phases exclude report serialization.
No phase durations are subtracted to invent overhead phases. Fresh process and
fresh project cache do not mean cold OS page cache. OS caches were not dropped.
Plain status is measured through captured pipes, not terminal paint/render time.
This is a live desktop with background applications, including a VM; task-owned
builds and other qualification processes are stopped for the final cohorts.
Interleaving reduces drift but cannot eliminate desktop scheduling noise.

## Reproduce

From the checkout, use the existing verified profiles and pinned SDK/providers:

```sh
cmake --build build/llvm23-release --target voidc voidls voidup -j 8
ctest --test-dir build/llvm23-release --output-on-failure -R '^(cli_contract|cli_presentation|voidup_smoke|voidup_presentation|project_profiles|project_managed_native|project_application_package|project_package_native|project_manifest_legacy|project_native_import|project_mixed_native)$'
ASAN_OPTIONS=detect_leaks=1:abort_on_error=1 UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1 ctest --test-dir build/llvm23-strict --output-on-failure -R '^(cli_contract|cli_presentation|project_profiles)$'
python3 -B tools/bench/compiler_project.py build/llvm23-release --lane both --controls --repetitions 20 --presentation legacy --output /tmp/g75-default-new.json
python3 -B tools/bench/compiler_project.py build/llvm23-release --lane native --controls --repetitions 20 --presentation timings-json --output /tmp/g75-detail-new.json
```

Use a fresh output filename for every benchmark. In `editors/vscode`, run
`VOIDLS_TEST_SERVER=/absolute/path/to/build/llvm23-release/voidls npm test`.
The existing `out/test/runDebugHost.js` and new `out/test/runG75Host.js` use
`VOIDC_PATH`, `VOIDLS_PATH`, `VSCODE_EXECUTABLE_PATH`, `VOID_LLDB_DAP`,
`VOID_LLDB_EXTENSIONS`; the G75 host also needs `VOID_G74_COMPILER` pointing at
the retained rebuilt baseline. `VOID_G75_MODES` can select
`slow,exception,zero,older`. Use existing local extensions and an actual native
editor; these scripts do not install the candidate. The exception replay uses a
disposable development harness in a normal editor because extension-test mode
refuses modal dialogs automatically.

VOIDUP's original acceptance/rejection/interruption scripts require current
signed private fixtures. The local wrapper retained in evidence redirects all
fixture roots, mutable control state and receipts into task-owned temporary
storage. Do not treat an expired catalog as a reason to bypass verification.

## Exact changed-file inventory

60 G75 files: 42 modified and 18 new. No staged paths.
Boss's concurrent `docs/WRENCH_CLANKER.md` change is separate from this inventory
and has been left untouched.
The original feature inventory contained 36 paths. The sentinel repair expanded
that inventory to 54; the authorized frontend/onboarding follow-up adds two
previously unchanged tracked paths and touches five paths in total. Its
[repair record](G75_DEBUG_CORRECTNESS_REPAIR.md#current-frontendonboarding-follow-up)
separates the accepted sentinel correction from this follow-up.
The reviewed 56-path candidate is preserved. The three-finding correction adds
the report-path helper, its focused test and its engineering review record.
The authorized re-review follow-up also corrects synchronization in the existing
project-selection test and workspace-scope matching for nested-project F5.

| State | Path |
| --- | --- |
| Modified | `CMakeLists.txt` |
| Modified | `apps/voidup/cli.void` |
| Modified | `apps/voidup/main.void` |
| Modified | `apps/voidup/manager.void` |
| New | `apps/voidup/presentation.void` |
| Modified | `docs/CLI.md` |
| Modified | `docs/EDITOR_INTEGRATION.md` |
| Modified | `docs/FIRST_PROGRAM.md` |
| Modified | `docs/README.md` |
| New | `docs/projects/G75_CLI_EXPERIENCE_ARCHITECTURE.md` |
| New | `docs/projects/G75_CLI_EXPERIENCE_IMPLEMENTATION.md` |
| New | `docs/projects/G75_DEBUG_CORRECTNESS_REPAIR.md` |
| New | `docs/projects/G75_GOLDEN_WRENCH_CORRECTIONS.md` |
| New | `docs/schemas/cli-timings-v1.schema.json` |
| Modified | `editors/vscode/README.md` |
| Modified | `editors/vscode/package.json` |
| Modified | `editors/vscode/src/config.ts` |
| New | `editors/vscode/src/debugBuildReceipt.ts` |
| Modified | `editors/vscode/src/extension.ts` |
| Modified | `editors/vscode/src/projectTask.ts` |
| New | `editors/vscode/src/test/debugBuildReceipt.test.ts` |
| New | `editors/vscode/src/test/g75LaunchHost.ts` |
| New | `editors/vscode/src/test/runG75Host.ts` |
| Modified | `examples/composition/order.void` |
| Modified | `examples/composition/receiver_order.void` |
| Modified | `examples/composition/stress.void` |
| Modified | `examples/interfaces/handoffs.void` |
| Modified | `examples/interfaces/stress.void` |
| Modified | `examples/managed_initialization/stress.void` |
| Modified | `examples/reference_values/order.void` |
| Modified | `examples/reference_values/required.void` |
| Modified | `examples/reference_values/stress.void` |
| New | `include/void/compiler_observation.hpp` |
| Modified | `src/backend/llvm/backend.cpp` |
| New | `src/cli_help.hpp` |
| New | `src/cli_presentation.hpp` |
| New | `src/cli_report_path.hpp` |
| Modified | `src/frontend/compiler.cpp` |
| Modified | `src/frontend/project_creation.cpp` |
| Modified | `src/frontend/project_files.hpp` |
| Modified | `src/frontend/project_resolution.cpp` |
| Modified | `src/frontend/project_selection.inc` |
| Modified | `src/main.cpp` |
| Modified | `src/native_build.cpp` |
| Modified | `tests/allocation_elimination/cases.hpp` |
| New | `tests/allocation_elimination/fixtures/terminator_escape.void` |
| Modified | `tests/binary/ownership.void` |
| Modified | `tests/channel/reload.void` |
| Modified | `tests/cli/contract.py` |
| New | `tests/cli/presentation.py` |
| New | `tests/cli/report_path.cpp` |
| Modified | `tests/integration/semantic_depth.py` |
| Modified | `tests/log/host.cpp` |
| Modified | `tests/math/host.cpp` |
| Modified | `tests/memory/ownership.void` |
| Modified | `tests/project/legacy.py` |
| Modified | `tests/project/selection.cpp` |
| Modified | `tests/stage10/console.cpp` |
| New | `tests/voidup/presentation.py` |
| Modified | `tools/bench/compiler_project.py` |

## Evidence and retention

Raw qualification records are retained at `/tmp/void-g75-evidence`. A verified ZIP with
DEFLATE level 9 is retained outside the repository:

`/home/shmellyorc/Documents/Void.Language-archives/g75-qualification-2026-11-18.zip`

SHA-256: `afb1c7c2f24d5a3fd4d930f8c9ab481dce0a804b18b9a84068c105e20144e116`. 810 selected members,
11,549,351 original bytes, 3,717,957 ZIP bytes. CRC and every
member SHA-256 were verified twice. `_ARCHIVE_MANIFEST.json` records original
paths and source/member hashes; the sibling `.index.json` records the archive
hash and cleanup. The maintained report is outside the ZIP to avoid a
self-referential archive hash.

The archive includes the original and final benchmark data, all raw samples,
complete CSV summaries, actual timing reports, schema-validation receipt,
selected passing/failing build/test/editor logs, baseline reproduction source,
VOIDUP process receipts, both actual interrupted 1 MiB payloads, public signed
catalog/inventory inputs, compiler binary hashes, and the changed working files
apart from this report. Private signing keys, installed toolchain copies and
large generated binaries/objects are excluded. The required original real
package inputs remain in the existing `build/voidup-fixtures`; matching full
package SHA-256 is recorded. Frozen G74 compiler/server binaries, the G74 Debug
reproducer/compiler, isolated current fixture inputs and unresolved failure
roots remain in task-owned scratch for review. Active source builds, SDKs,
providers, user installations/settings and unrelated files were retained.

After verification, four successful task-owned VOIDUP acceptance/interruption
roots were removed. Their exact paths, directory identities, file counts and
byte totals are in the archived cleanup plan and sibling archive index. Unique
partial bytes and crash receipts were archived before removal; complete package
copies matched the retained fixture. `/tmp` free space increased from
6,313,263,104 to 12,321,435,648 bytes, a measured gain of
5.60 GiB. A superseded task-created ZIP was
removed only after the final ZIP passed verification. No major repository or
unrelated cleanup was performed.

Final checks: unchanged HEAD/local `origin/main`, empty staged diff, 56-path
working inventory, no whitespace errors and no missing relative document link
targets in the edited canonical guides/report. Native Windows/macOS acceptance,
independent Wrench review and Boss's JSON-cost decision remain outstanding.


The later [Debug repair record](G75_DEBUG_CORRECTNESS_REPAIR.md) retains the earlier
22-path sentinel closeout and its verified archive. Its subsequently authorized
frontend/onboarding follow-up touches five repository paths and passes fresh
complete Debug 516/516, Release 503/503 and strict 493/493 matrices, with zero
failures/skips and the required 1 MiB frontend limits unchanged. All three
sentinel-affected cases retain their expected results. The non-reproducing AOT
build event and Steam/RimWorld context are preserved; Boss explicitly accepted
recording that event as non-blocking. Source/fixture preservation, code-generation
comparisons, performance evidence and exact 56-path inventory are recorded there.
G75 feature work awaits integrated review; G69 remains on hold and G76 is queued.
