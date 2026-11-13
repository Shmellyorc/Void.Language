# G19 — Completion attribution and bounded reuse

12 November 2026. Baseline `3c81b7eb75c44533a5326da589a6fe47144c3c0f`.
**IN PROGRESS pending Golden Wrench review. Production optimization HOLD.**

## Decision and scope

**HOLD production optimization.** The one isolated candidate makes ordinary
member completion materially faster on Linux, but changes observable failure
behavior when the selected project root disappears. Exact error equivalence is
a required G19 gate. No production file was modified and no second candidate was
attempted. Native Windows qualification uses the existing VM labelled “Windows
11”; its actual guest is Windows 10 Home x64, build 19045.6466.

The measured hotspot is **fresh dependency/lock verification twice per member
completion**, followed by graph loading and semantic binding. Candidate collection
and JSON response construction are small in this workload. This is not evidence
that dependency verification can be skipped or cached by editor revision.

G18 is integrated and independently source-reviewed at the baseline, per the
assignment. Its separate Windows executable-cleanup refusal is not attributed
to completion performance. No hosted CI, Git mutations, production cache,
language change, Runtime change or persistent Windows disk cache is included.

## Preserved pre-experiment design record

The following design record predates the experiment. Its pending items are
resolved or explicitly limited in the final qualification sections below.

The frozen G08 fixture and its original benchmark are unchanged. New native
Linux cohorts have 20 server sessions and 200 repeated member completions per
profile: Release median 14.069 ms, p95 15.980, worst 26.340; Debug median
69.541 ms, p95 75.799, worst 99.288. These are new G19 observations, not edits to
G08's original numbers. They exclude client rendering.

An isolated Release attribution replay identifies two fresh project captures
and two dependency verifications on this query. Preliminary per-request median
exclusive dependency verification is 6.964 ms; binding is 2.437 ms, graph
loading 2.564 ms, and candidate collection 0.203 ms. These independent medians
are not an additive workload model. Final matched receipts will supersede this
preliminary interpretation without removing the baseline observations.

The single experimental candidate is **deferred first capture for an exact
overlay member probe ending immediately after `.`**. Ordinary import, signature,
partial-name, package-source and independent-document routes keep their current
path. The experiment prepares cursor text from the supplied immutable overlay
and then calls the existing manifest analyzer, which performs fresh root,
manifest, dependency/lock, source authorization and binding before candidates
can escape. It does not cache an analysis or dependency between requests.

The potentially redundant operation is the earlier capture used to obtain
bytes already present in the overlay. The later capture must remain mandatory.
The expected ceiling is one of the two capture/validation costs, not all of
dependency validation or binding. A speedup is unproven until matched runs.

Ownership remains request-local: existing SourceFile owners, frozen overrides,
ProjectCapture and binder lifetimes. No retained-memory cache is introduced.
The early lexical path must not return keywords/import items or borrow authority
from an old editor revision. Eligibility is deliberately narrow; malformed
source, visibility, entry selection and stale locks need exact result-equivalence
controls. A dependency mutation before the remaining capture must be rejected.

Risks include diagnostic/empty-result precedence, first-capture errors formerly
preventing any probe work, local disk sources versus intentional editor overlays,
and native path-admission differences. Same-revision dependency mutation,
source/manifest/lock/entry/dirty/close controls and G14/G16/G18 regressions are
required. Instrumented baseline and candidate must preserve ordered completion
objects, source positions and diagnostics, not just labels. Off/on observation
controls must bound tracing perturbation.

Rollback is deletion of the isolated experimental server and retention of the
unchanged production path. No production source has been edited. Cross-platform
and native editor qualification remain pending; no implementation gate is passed
by this initial design record.

## Actual path and attribution boundaries

Source references below refer to the exact baseline, not generated overlays.

| Component | Responsible source | Observed work |
| --- | --- | --- |
| Request and snapshot | `src/lsp/main.cpp:537`, `:581`, `:605`, `:614`, `:877` | Select current project/editor snapshot; merge source owners and overlays; invoke compiler assistance; encode reply. |
| Initial capture | `src/frontend/compiler.cpp:7738`; `src/frontend/project_selection.inc:43` | Admit project root, manifest, dependency lock and source graph before reading/probing source. |
| Dependency verification | `src/frontend/project_selection.inc:81`; `src/frontend/project_resolution.cpp:577` | Fresh lock/tree/content validation and held-root checks; retained verified source metadata. Two calls in the measured member request. |
| Probe preparation and physical repair | `src/frontend/compiler.cpp:7649` | Lex original text; reject unsupported/trivia contexts; build cursor sentinel and bounded delimiter repair; freeze changed source. |
| Repaired graph capture/load | `src/frontend/project_selection.inc:369`, `:398`, `:439` | Second admitted capture, parse/load graph and selected entry. |
| Binding and candidates | `src/frontend/compiler.cpp:6176`, `:2747` | Bind graph with compiler-owned editor probe; collect typed visible candidates and exact replacement positions. |
| Ordinary final recapture | `src/frontend/project_selection.inc:450` | **Not executed on successful measured member probes:** `EditorProbeFound` escapes first. Do not count a third capture. |

`ToolingIndex`, the session snapshot and `SourceFile` owners remain the existing
lifetime mechanisms. No mutable binder or AST is shared across requests by G19.
Project revision guards are necessary but not sufficient: files outside editor
notifications can change dependency authorization.

The isolated RAII instrumentation records nested **inclusive and exclusive**
wall time. The receipt timestamp starts after a complete framed body is read;
it excludes waiting for the first incoming byte. Dispatch includes compiler
work, response serialization and pipe write, not editor rendering. The client
round trip additionally includes scheduling, transport and Python reader work.
No standalone transport microbenchmark was subtracted from these observations.
No optimization experiment changes the Windows polling/transport mechanism.

`repair` measures the physical delimiter-repair block. G14's deeper parser/binder
recovery remains within graph loading/binding. The frozen G08 member workload is
valid source, so this does **not** separately attribute every invalid-source
recovery route. `candidates` covers the common member collector, not all import,
enum or keyword-specific routes. Scope counters establish which path ran.
Dependency scopes are exclusive of their containing captures; do not add
inclusive capture time to dependency time.

Tracing is enabled only with `VOID_G19_TRACE` in a disposable server. The JSONL
write occurs after the measured dispatch interval; observer-off runs check its
client-visible perturbation. Trace output is not a production feature.

## Conditions and new baseline

The frozen `tools/bench/compiler_project/` inputs and original
`tools/bench/compiler_project.py` are unchanged. Every cohort retains file hashes,
resolved lock hash, compiler/server hashes and tool versions. Normalized transcript
comparison removes only the disposable absolute project root, preserving labels,
ordering, documentation, source-relative identities, UTF-16 ranges and diagnostics.
No compilation, linking, fixture resolution or prior session is included in a
completion request timer. The nine-file fixture aggregate SHA-256 is
`063a086a45584eed8d7740d9641efd164ce8125593c09d0c4a74ac31d3646ab3`;
the resolved lock is
`f4d776e773bcd7d3fef845b90e92bb78279d6830fd477e0c5e49c017edb1332f`.
Both match historical G08 on Linux and Windows. Twenty fresh server sessions per baseline profile,
ten repeated queries per session, produce 200 repeated observations. Caches and
request order match the original editor lane.

Linux uses the existing PikaOS x64 Debug/Release builds, LLVM/Clang/LLD 23.1.4,
and CPU affinity 4–7 for the protocol timing cohorts. Strict sanitizers measure
correctness only. Windows uses native x64 Clang-CL through the logged-in VS2022
x64 developer environment: MSVC 14.44.35207, SDK 10.0.19041.0, pinned LLVM 23.1.4,
profile-matched verified host-provider and Schannel prefixes. Guest builds and
fixtures are on local NTFS, not the VirtualBox shared folder. Each Windows temp
root passed the existing `void_windows_native_staging --create-private-root`
owner/DACL verifier. The shared folder transports scripts and receipts only.

The Windows host receipt names the original attested repository path. G19 reused
the established CMake include shim only after comparing every verifier recipe
byte and the shim against the current source; the unchanged verifier validated
the original physical dependency prefixes. No receipt or trusted principal was
changed. Raw configuration logs retain these checks.

Initial Linux Debug baseline partly overlapped a separate compiler build;
initial Windows Release partly overlapped a Linux editor correctness run. Keep
these observations, but do not infer source regressions from comparisons against
historical G08 or treat tails as isolated-machine guarantees. The matched Linux
candidate blocks ran serially without those builds. Later observation-off native
cohorts provide additional controls. VM and host share physical resources; this
is local qualification, not a universal Windows/Linux ranking.

| Profile | Repeated completion median / p95 / worst ms (n=200) | First completion ms (n=20) | Spawn ms (n=20) | Initialize ms (n=20) | Peak RSS/working set KiB (n=20) |
| --- | --- | --- | --- | --- | --- |
| PikaOS Release | 14.069 / 15.980 / 26.340 | 14.069 / 16.509 / 23.492 | 0.595 / 0.717 / 1.265 | 1.598 / 1.874 / 2.274 | 23744.000 / 24120.000 / 24352.000 |
| PikaOS Debug | 69.541 / 75.799 / 99.288 | 69.656 / 74.282 / 80.938 | 0.674 / 0.779 / 0.784 | 1.869 / 2.612 / 2.884 | 23984.000 / 24152.000 / 24156.000 |
| Windows 10 Release | 53.482 / 60.230 / 83.305 | 53.207 / 58.586 / 59.972 | 8.136 / 10.047 / 56.707 | 6.100 / 8.609 / 8.787 | 23544.000 / 23856.000 / 23912.000 |
| Windows 10 Debug | 98.923 / 103.445 / 115.655 | 99.845 / 102.317 / 104.544 | 9.326 / 11.854 / 63.005 | 6.213 / 7.082 / 10.198 | 24638.000 / 25200.000 / 25220.000 |

Memory is end-of-session RSS/high-water RSS on Linux and working set/peak working
set on Windows, in KiB. These are different OS accounting measures, not a proof
of equivalent heap size. Windows uses `GetProcessMemoryInfo` with the documented
query-limited access right; no ptrace or credential guessing was needed.
[Microsoft API contract](https://learn.microsoft.com/en-us/windows/win32/api/psapi/nf-psapi-getprocessmemoryinfo).

## Disjoint phase results

| Exclusive phase, arithmetic mean ms | Linux Release (240) | Linux Debug (60) | Windows Release (60) | Windows Debug (60) |
| --- | --- | --- | --- | --- |
| dispatch | 0.007006 | 0.015800 | 0.017415 | 0.021995 |
| selection | 0.028980 | 0.108673 | 0.089770 | 0.242322 |
| first_capture | 0.236628 | 0.810983 | 1.795347 | 2.488982 |
| second_capture | 0.182577 | 0.737334 | 1.485262 | 2.266792 |
| dependency | 7.103127 | 25.490517 | 18.443857 | 22.426532 |
| probe | 0.539868 | 2.270656 | 1.536160 | 3.500147 |
| repair | 0.029093 | 0.259290 | 0.042848 | 0.230600 |
| graph_load | 2.598179 | 22.028461 | 12.108983 | 30.282702 |
| binding | 2.479457 | 18.466147 | 3.924285 | 23.049700 |
| candidates | 0.206236 | 0.417566 | 0.160118 | 0.406933 |
| result | 0.025757 | 0.071127 | 0.021383 | 0.057985 |
| response | 0.022758 | 0.050030 | 0.056578 | 0.074140 |
| recapture | 0.000000 | 0.000000 | 0.000000 | 0.000000 |
| Measured dispatch total | 13.459854 | 70.726950 | 39.682447 | 85.049542 |
| Receipt to dispatch, separate | 0.007139 | 0.027906 | 0.035603 | 0.056253 |
| Unattributed scope bookkeeping | 0.000187 | 0.000367 | 0.000440 | 0.000713 |

For the initial Release attributed cohort, 240 successful member requests (first,
repeats and restored-dependency queries) have mean dispatch **13.459854 ms**.
The disjoint exclusive means sum to dispatch within **0.000187 ms** of scope
bookkeeping. Receipt-to-dispatch adds **0.007139 ms** on average. This is a
same-cohort additive model; independent medians are not added.

The two inclusive capture means are **3.910487 / 3.611845 ms**, already including
their respective dependency verification. Together they account for approximately
56% of server dispatch. Exclusive dependency verification totals **7.103127 ms**
(two calls); graph load **2.598179 ms** and binding **2.479457 ms** follow.
Candidate collection is **0.206236 ms** and result construction **0.025757 ms**.
Thus capture/verification is the largest component here, while binding remains
consequential. This does not generalize to dependency-free or much larger graphs.

Linux Debug observer-on median/p95/worst was **71.637 / 73.872 /
79.071 ms**, versus observer-off **70.618 / 73.513 / 81.255 ms**
(50 each); observer-on was 1.4% higher at the median.

Linux Release observer-on repeated-query median/p95/worst was
**13.579 / 14.362 / 27.267 ms** (200); observer-off was
**13.542 / 14.241 / 14.850 ms** (100). Median difference is small, but the high
observed tail and unmatched cohort sizes prohibit a claim of zero perturbation.

Windows Release observer-on median/p95/worst was **52.861 / 67.323 /
95.285 ms**, observer-off **53.410 / 60.804 / 70.722 ms** (50 each).
Windows Debug was **98.943 / 112.458 / 126.032 ms** on and
**98.614 / 107.645 / 135.282 ms** off (50 each). All observed and unobserved
standard transcripts matched. The differing tails remain part of the evidence.

For the same first-plus-repeat request population (excluding untimed restored
queries), mean client round trip minus mean measured server receipt/dispatch is
**0.251 ms Linux Release**, **0.287 ms Linux Debug**, **15.447 ms Windows
Release** and **15.327 ms Windows Debug**. This residual includes pipe waiting,
OS scheduling, client reader work and instrumentation effects; it is not a
measurement of pure editor rendering or proof that one syscall owns all of it.
G08 already documents Windows polling/wait behavior. No new transport candidate
was attempted, and no transport estimate was added to the independent phase table.

On Windows Release, exclusive dependency work is 18.444 ms and graph loading
12.109 ms; on Windows Debug, graph loading (30.283 ms) exceeds either binding
(23.050 ms) or dependency verification (22.427 ms). Repeated capture is therefore
not a universal explanation for every profile. The Linux Release finding must
not conceal profile and platform differences.

## One isolated candidate and its rejection

The disposable variant defers the initial capture only when exact overlay bytes
are present for an ordinary non-package member probe immediately after `.` and
outside a `use` line. It skips import assistance on that narrow route and passes
the frozen source to the existing manifest analyzer, which still captures,
verifies and binds before any candidate escapes. Other contexts remain on the
original path. There is no retained cache or revision-only reuse.

The experiment is generated by `completion_attribution.py --candidate` outside
the checkout. Linux compiles only the instrumented compiler and LSP translation
units using the configured recipes, placing their objects before unchanged
archives. Windows attribution uses baseline-only overlays in the private guest
source tree and native CMake builds. **No Windows candidate speedup is claimed:**
the Linux failure gate already rejected it, so additional candidate performance
qualification there was not justified.

Matched Linux observation-off ABBA blocks (baseline, candidate, candidate,
baseline) each contain five sessions and ten repeated queries. Each variant has
100 repeated samples per profile. Same source, SDK, sequence and physical host:

| Profile | Production median / p95 / worst ms | Candidate median / p95 / worst ms | Median change | Median peak RSS KiB, production → candidate |
| --- | --- | --- | --- | --- |
| Release | 13.846 / 14.984 / 21.931 | 9.992 / 11.325 / 11.776 | 27.8% lower | 23,784 → 23,174 |
| Debug | 69.949 / 71.360 / 73.968 | 57.953 / 58.928 / 64.845 | 17.2% lower | 24,134 → 23,664 |

Both baseline blocks remain slower than both candidate blocks in each profile.
This is a repeatable improvement for the narrow protocol workload, not proof of
an end-user editor speedup or general scaling. Retained memory is bounded over
these sessions; no long-lived-cache claim is made.

All successful standard replay transcripts matched exactly (normalized SHA-256
`49cabf9f561ce094c3d5ea72c7224002d4790c165970d178ba12fda206da8c7c`).
The twelve authority scenarios also matched Linux baseline versus candidate.
Windows Debug/Release authority receipts match each other; cross-host differences
are limited to the physical standard-library URIs in cleared diagnostics, which
are intentionally retained rather than normalized away. However,
that coverage was **insufficient**:

| Controlled root-disappearance operation | Production | Isolated candidate |
| --- | --- | --- |
| Open project and source, rename physical project root, request completion using old URI | JSON-RPC `-32603`, `Internal server error`; server exit 2 | Empty result `[]`; server remains alive |

Root admission in the first `ProjectCapture` throws `filesystem_error` outside
the analyzer's catch boundary. Deferral moves that same failure into
`analyze_manifest_project_impl`'s catch boundary and changes transport/exit
behavior. The transport fatal handler is `src/lsp/main.cpp:954`; the analyzer converts
filesystem errors into diagnostics at `src/frontend/project_selection.inc:473`.
This is a demonstrated exception-ordering incompatibility, not a
hypothetical stale-cache concern. The original exploratory harness also encountered
EOF when it attempted shutdown after the production exit; a single controlled
replay captured the exact response, exit, stderr and source as `baseline_crash.void`.
It was **not** SIGSEGV and is not claimed repaired. The initial EOF observation is recorded here; the controlled
response, exit, stderr and exact source are preserved in the archive. Do not silently normalize away error/exit
behavior to claim equivalence.

No candidate revision was pursued. Any future first-capture sharing design must
preserve the original root/manifest/dependency admission and failure boundary
before probe work, as well as source ownership and final authority. G19 does not
authorize such a redesign or a revision-only cache. Rollback is removal of the
external experimental binaries; production already remains unchanged.

## Authority and correctness matrix

`completion_authority.py` records complete responses before mutation, immediately
after mutation at the same revision, and after explicit project refresh.

| Control | Required and observed behavior |
| --- | --- |
| Same-revision repeated query | Ordered candidates unchanged. |
| Locked dependency bytes / invalid lock / invalid manifest | Immediate empty completion at unchanged selection revision. Fresh authorization cannot be replaced by revision equality. |
| Referenced / unreferenced local disk source | Immediate query observes the current editor snapshot; explicit refresh incorporates disk change. Snapshot authority and disk authority are not conflated. |
| Entry switch | Existing selected-entry and visibility behavior preserved. |
| Dirty source and stale older version | Current dirty source wins; old revision rejected; disk bytes unchanged. |
| Closed / removed source | Existing close and refresh semantics retained; source ownership remains with existing snapshot machinery. |
| Invalid import / visibility change | Existing recovery and visibility decisions preserved. |
| Physical root disappears | **Candidate fails equivalence; production HOLD.** |

The standard G08 replay additionally covers dirty manifests, selection, renamed
and restored modules, dependency mutation, hover, semantic tokens and diagnostics.
G14/G16/G18 regressions cover incomplete nested/enum contexts, UTF-16, visibility,
constructor/signature completion, recovery, diagnostic spans and guarded fixes.
No arbitrary filesystem race or allocation-failure exhaustiveness is claimed.

| Profile / unchanged production | Focused CTests | Outcome |
| --- | --- | --- |
| PikaOS Release | 15/16 passed | Existing `lsp_stage7` tab-formatting assertion failed; no test disabled. |
| PikaOS Debug | 15/16 passed | Existing `lsp_stage7` tab-formatting assertion failed; no test disabled. |
| Linux strict ASan/UBSan/leak | 15/16 passed | Existing `lsp_stage7` tab-formatting assertion failed; no test disabled. |
| Windows 10 Release | 15/16 passed | Existing `lsp_stage7` tab-formatting assertion failed; no test disabled. |
| Windows 10 Debug | 15/16 passed | Existing `lsp_stage7` tab-formatting assertion failed; no test disabled. |

The unchanged `lsp_stage7` fixture fails at `tests/lsp/stage7.py:86`: it expects
`-32602` for `insertSpaces:false`, but the baseline server supports tabs and
returns a result. The resulting `KeyError: 'error'` is preserved; the assertion
was neither removed nor changed. It truncates the later checks in that one test.
Do not report a wholly green focused suite or reopen formatting implementation
as a G19 repair. Other focused fixtures run independently.

The isolated Linux candidate passed eight focused protocol/recovery/intelligence
regressions in each of Release, Debug and strict ASan/UBSan/leak mode. Passing
those checks does not override the later root-error failure. Strict options were
`ASAN_OPTIONS=detect_leaks=1:halt_on_error=1:exitcode=87` and
`UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1:exitcode=86`.

## Native editor observations

Native VS Code **1.141.0** passed the frozen G08 project replay on both PikaOS
and Windows 10 using the unchanged Release server: local/imported scope symbols,
member completion, rendered popup, dirty incomplete expression, unchanged disk,
explicit refreshed dependency refusal and restoration. Ten provider-command
round trips measured Linux median **51.519 ms**, worst **85.879 ms**, and Windows
median **60.667 ms**, worst **64.994 ms**. These small UI cohorts exclude popup
polling, have different host scheduling conditions, and are **not** a matched
baseline/candidate editor-speed comparison.

The native G14 rendered-popup/semantic-color/navigation/dirty-diagnostic harness
and G16 enum-popup/semantic-role harness passed on both platforms. Linux used
the isolated Release candidate for these two additional compatibility checks;
Windows used the preserved unchanged Release server. The candidate's protocol
speedup is not advertised as a measured UI improvement. Debug correctness is
covered by the native protocol suites, not a separate Debug UI timing cohort.

Windows reused its verified cached native editor. Its application metadata lives
under the versioned `2a59476c9b/resources/app` directory; two initial preflight
attempts expected a flat layout and failed before launching editor tests. The
successful preflight checked the actual package version, and the test receipt
also reports `vscode.version`. No editor download or access-control bypass was
needed. A read-only archive search was stopped after locating the existing
versioned installation; it did not affect any timed cohort.

The new frozen-project editor harness initially assumed `rates.` supplied member
completion. Direct baseline protocol inspection returned `[]`; ordinary scope
completion contains `rates`, `bulk_threshold` and `unit_discount`. The corrected
harness asserts imported scope symbols and explicitly records the unsupported
namespace-member result, rather than changing production to satisfy the test.

A subsequent editor check timed out waiting for an empty result after an external
dependency edit. The final control disables VS Code word-based suggestions in its
private profile and uses the existing explicit `void.refreshProject` command.
These two harness changes were not separately attributed; do not assert the
original timeout was definitively caused by word suggestions or that automatic
watcher latency was qualified. The direct protocol same-revision dependency
refusal remains separately verified without refresh. The final editor test proves
refusal after refresh, dirty-buffer behavior, restored authority and a rendered
member popup. All earlier failing harness logs remain in the evidence archive.

## Reproduction and evidence

Use the baseline source and existing verified LLVM 23.1.4 profile. From repository
root, create a private task-owned directory outside the checkout, then run:

```sh
TMPDIR="$g19_scratch" taskset -c 4-7 python3 -B tools/bench/completion_replay.py \
  build/llvm23-release --repetitions 20 --queries 10 --output "$g19_scratch/baseline.json"
python3 -B tools/bench/completion_attribution.py build/llvm23-release "$g19_scratch/attributed"
TMPDIR="$g19_scratch" taskset -c 4-7 python3 -B tools/bench/completion_replay.py \
  build/llvm23-release --server "$g19_scratch/attributed/voidls" \
  --trace "$g19_scratch/phases.jsonl" --output "$g19_scratch/observed.json"
TMPDIR="$g19_scratch" python3 -B tools/bench/completion_authority.py \
  build/llvm23-release/voidc build/llvm23-release/voidls --output "$g19_scratch/authority.json"
```

The attribution builder currently consumes configured Linux Makefile link recipes;
it is not a portable new build framework. Its exact source anchors fail closed.
The rejected `--candidate` switch exists only to reproduce this experiment and
is explicitly labelled unsafe for production. JSON output names must be fresh.
Run the same instrumented server without `--trace` for observer-off controls.
The editor harness uses the existing `@vscode/test-electron`, extension, native
VS Code 1.141.0 executable and existing CDP workbench helper; no dependency added.

For Windows, reuse the verified native x64 developer environment and physical
pinned prefixes. Use the existing native private-root verifier for TMP/TEMP.
The archived guest scripts show exact configuration, native overlay rebuild,
profiles and commands. Do not substitute shared-folder builds or Linux binaries.
The preserved root-boundary fixture/receipt is Linux-only. To reproduce, resolve
the frozen project, initialize a project-mode server with that project as cwd,
open `src/stock.void` at version 1 and cross the existing barrier. Rename the
physical project directory while the server is alive, then request completion
using the original URI at zero-based line 4, UTF-16 character 42 (immediately
after the first `item.`). Capture response, process exit and stderr before
restoring the directory. Preserve the exact submitted source on abnormal exit.
Windows rename restrictions are not bypassed to manufacture an equivalent test.

Raw observations, exact compared transcripts, source/fixture hashes, generated
instrumentation, build recipes, all unsuccessful setup/editor/test receipts and
the root-boundary fixture are retained in:

- `/home/shmellyorc/Documents/Void.Language-archives/g19-completion-2026-11-12.zip`
- Adjacent `g19-completion-2026-11-12-index.json` and
  `g19-completion-2026-11-12-cleanup.json`.

Archive SHA-256:
`92f1d99469b5457924fc65663e46648f3e67fe64e233706a0d0c1fd70a92cce9`.
**215 evidence/source members**, 12,718,015 original bytes, **2,530,165 ZIP
bytes**; DEFLATE level 9. CRC and every extracted manifest member SHA-256 were
verified before cleanup. `_ARCHIVE_MANIFEST.json` and `RESTORE.md` describe
restoration. Binary/toolchain copies are intentionally omitted; source packets
can be reconstructed from the baseline and preserved per-file hashes.

Verified task-owned cleanup removed 1,722,457,470 logical bytes from native
`C:\Users\a\VoidG19-20261112` (no reparse points), 354,106,375 bytes from
`/tmp/void-g19-0qgzmbi5`, and 68,842,828 bytes from the task's VM transfer folder.
All three paths were verified absent. Guest free space rose from 13,228,793,856
to 14,978,113,536 bytes; host temporary-volume free space rose from 16,222,138,368
to 16,576,815,104 bytes. Shared-folder volume free space rose from
367,312,039,936 to 367,327,006,720 bytes. Free-space changes are filesystem
observations, not expected to equal logical byte totals. Existing SDKs, provider
prefixes, native editor caches and unrelated builds were retained. The native
Windows guest remains available; it was not renamed or shut down.

## Inventory and review boundary

Maintained changes are this report, the G08 dated follow-up, documentation index,
feature roadmap, milestone register and four developer-only benchmark files:
`completion_attribution.py`, `completion_replay.py`, `completion_authority.py`,
`completion_editor.cjs`. The frozen fixture, original benchmark, production source,
build configuration, extension implementation and existing tests are unchanged.

Final checks: `git diff --check` passed; HEAD remains
`3c81b7eb75c44533a5326da589a6fe47144c3c0f`. Read-only diff inspection confirms no
changes under production source, includes, standard library, tests, CMake,
editors, root README, original benchmark or frozen fixture. Python measurement
scripts passed syntax compilation without bytecode writes; the editor harness
passed native Node syntax checking. Relative links in the changed documents
resolve. No staging, commits, branches, pushes or hosted CI occurred.

Exact final worktree inventory (all uncommitted):

```text
 M docs/README.md
 M docs/compiler-health/G08_COMPILER_PERFORMANCE.md
 M docs/roadmap/FEATURE_COMPLETENESS_AND_DEVELOPER_EXPERIENCE.md
 M docs/roadmap/MILESTONE_REGISTER.md
?? docs/compiler-health/G19_COMPLETION_ATTRIBUTION.md
?? tools/bench/completion_attribution.py
?? tools/bench/completion_authority.py
?? tools/bench/completion_editor.cjs
?? tools/bench/completion_replay.py
```

Next recommendation: Golden Wrench should review the attribution and the
exception-boundary rejection together. Retain the developer measurement seam
and authority controls if accepted. Do not promote the candidate or introduce a
revision-only cache. Any later capture-sharing proposal must keep the original
admission/error boundary and demonstrate authority and failure equivalence
before another production implementation decision; G19 establishes no approval
for that additional work.

G19 remains **IN PROGRESS pending Golden Wrench review**, with **production
optimization HOLD**. The deliverable establishes a hotspot and a measured but
rejected candidate. It does not establish a safe production optimization.
No new release gate is invented. G00-A redistribution, G08 optimization,
Windows persistent cache, G06 symlink privilege, G10 deadline composition,
G11 retained raylib callback, unsupported macOS managed-native/D5, advisory
FILE200 history and Boss-deferred seven-job integration gate remain unchanged.
