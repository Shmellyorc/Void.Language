# CI ThinLTO link experiment — performance evidence and acceptance boundary

**Review date:** 24 October 2026 (UTC).  
**Ownership:** 👑 Boss authorizes Git/CI and production changes; 🔧 Golden Wrench independently reviews; 🔧 Silver Wrench supervised the isolated investigation; 🏮 Research Clanker investigated LLVM; ⌨️ Keyboard Clanker implemented the experiment.  
**Decision:** **Preserve the experimental workflow; do not adopt ThinLTO caching in main CI yet.** The build-time improvement is material. Cross-platform correctness and controlled attribution remain open.  
**Stage 10:** **INCOMPLETE.** This report does not supersede Step 41 Linux acceptance or Step 42 native gates.

## 1. Authoritative links and exact source scope

| Evidence | Link / identity |
|---|---|
| Historical successful CI #95 | [Run 39724345062](https://github.com/Shmellyorc/Void.Language/actions/runs/39724345062), commit `e25816bdb80e3f32613edf77c9cf3258232af966` |
| Main CI #96 | [Run 40042618839](https://github.com/Shmellyorc/Void.Language/actions/runs/40042618839), commit `ef6017613ecf92de31380476ed5d9c3fc8cf3dbb`; three Linux jobs hit the former 180-minute job limit while linking |
| Main CI #97 | [Run 40069548583](https://github.com/Shmellyorc/Void.Language/actions/runs/40069548583), commit `96bac2ea8e4be31d574a5916118feab0ef6b2e7f`; independently observed Ubuntu Release and Windows jobs green at review, remaining jobs then in progress |
| Experimental source | Commit [`8485e6404596cf73deb55302ad3678a23cc0c6a4`](https://github.com/Shmellyorc/Void.Language/commit/8485e6404596cf73deb55302ad3678a23cc0c6a4) |
| Same-commit **control** experiment | [Run 40074433020](https://github.com/Shmellyorc/Void.Language/actions/runs/40074433020) |
| Same-commit **cache** experiment | [Run 40075098927](https://github.com/Shmellyorc/Void.Language/actions/runs/40075098927) |
| Experiment implementation | [`.github/workflows/ci-link-experiment.yml`](https://github.com/Shmellyorc/Void.Language/blob/8485e6404596cf73deb55302ad3678a23cc0c6a4/.github/workflows/ci-link-experiment.yml) |
| Official primary CI | [`.github/workflows/ci.yml`](https://github.com/Shmellyorc/Void.Language/blob/8485e6404596cf73deb55302ad3678a23cc0c6a4/.github/workflows/ci.yml), unchanged by the experiment |

**Evidence distinction:** Silver Wrench's handoff supplied original archive/member counts and precise historical-versus-cached build timings. Golden Wrench independently checked the committed experimental workflow, CI job outcomes and relevant native failure logs. Exact `.ninja_log` timings, full resource metrics and cached-object identities are recorded in experimental job artifacts; the values quoted below are from the handoff unless separately stated. The cached experimental run later completed: two Windows jobs passed, while Ubuntu Debug/Release/Sanitizers and macOS Debug/Release ended with the test failures recorded below. Later follow-up runs must be appended without rewriting these historical observations.

## 2. Why linking dominated

CI #95 completed all seven jobs, but individual build stages were expensive:

| Configuration | CI #95 build (historical) |
|---|---:|
| Windows Release | 3m 41s |
| Windows Debug | 5m 16s |
| Ubuntu Release | 1h 04m 36s |
| Ubuntu Debug | 2h 13m 37s |
| macOS ARM64 Release | 43m 53s |
| macOS ARM64 Debug | 1h 58m 56s |

Approximately **427 executable-link commands** were present in Linux/macOS build inventories. The minimal CMake `VOID_LLVM_ORC_RUNS` configure-time probe itself reportedly cost **124 s** (Ubuntu Release), **212 s** (Ubuntu Debug), **133 s** (macOS Debug), versus roughly **9 s** (Windows Debug).

Research Clanker's independent Linux archive inspection reported ThinLTO bitcode members in `LLVMCore` (**75**), `LLVMOrcJIT` (**41**), `LLVMX86CodeGen` (**66**), and `LLVMSupport` (**151 bitcode + 4 native ELF**). Sampled bitcode identified ThinLTO summaries. The official LLVM 19.1.7 macOS release configuration also uses ThinLTO, though the macOS ARM64 archive members were not independently inventoried in this investigation.

**Hypothesis, not proof:** repeated LLVM-dependent links repeatedly optimize/code-generate the bundled SDK's bitcode. Caching an SDK download does not reuse generated native code. Three concurrent large links can add contention, but the magnitude of that effect was not measured. Do not attribute the entire speedup to any one mechanism without a completed controlled comparison.

Official references: [Clang/LLVM ThinLTO incremental builds](https://releases.llvm.org/19.1.0/tools/clang/docs/ThinLTO.html#incremental), [LLVM 19.1.7 LLD ELF LTO implementation](https://github.com/llvm/llvm-project/blob/llvmorg-19.1.7/lld/ELF/LTO.cpp), and [LLVM 19.1.7 bitcode reader](https://github.com/llvm/llvm-project/blob/llvmorg-19.1.7/llvm/lib/Bitcode/Reader/BitcodeReader.cpp#L8491).

## 3. Isolated experimental workflow and invariants

One new, manually dispatched workflow provides `control` and `thinlto-cache` modes. Both select the same seven OS/configuration jobs on the **same source commit** when dispatched from the same ref.

- **Linux cache mode:** `-Wl,--thinlto-cache-dir=<absolute-job-local-directory>`. For early CMake compiler/ORC checks, it **also** repeats the existing pinned LLVM 19.1.7 `--ld-path` selection in `CMAKE_EXE_LINKER_FLAGS` so GNU ld does not encounter the cache option. This is a *second controlled-mode difference* and must be distinguished when interpreting Linux A/B effects. It does not upgrade or replace the production linker.
- **macOS cache mode:** `-Wl,-cache_path_lto,<absolute-job-local-directory>` through the existing executable link flags.
- **Windows:** no ThinLTO cache link flags; existing behavior remains unchanged in either mode. Windows is a control for workflow overhead, not a test of Unix ThinLTO gains.

Each job starts with a **new empty directory** under the runner's temporary root. The configure-time ORC test can prime entries before the main build; subsequent executable links reuse them. There is **no cross-run persistent cache** or new cache service. An optional representative warm `voidc` relink is scheduled **after successful acceptance**, so on a failed CTest step it is skipped.

Preserved from the main contract: pinned LLVM/Clang/LLD **19.1.7**, supported C++ ABI, Debug/Release/strict configurations, all existing registered CTest suites, sanitizer flags, original 1 MiB stack probes, editor integration, packaging, three build workers, 360-minute job budget, and existing LLVM optimization settings. No application `-flto` switch, production compiler change, new language/runtime component, or disabled test is part of the experimental workflow.

The workflow records `identity.json`, CMake trace and ORC-probe wall time, `configure.json`, `build.json`, raw configure/build logs, Ninja targets, `.ninja_log` and per-link CSV durations, cache directory file/byte totals, child resource measurements where available, test JUnit/logs, and post-acceptance `relink.json` when reached. Resource measurements must not be mistaken for concurrent aggregate host RSS.

## 4. Performance findings and validity

| Configuration | Historical CI #95 | Experimental cache-mode build | Historical ratio |
|---|---:|---:|---:|
| Ubuntu Release | 64m 36s | **9m 00s** | ~7.2× |
| Ubuntu Debug | 133m 37s | **9m 59s** | ~13.4× |
| macOS ARM64 Debug | 118m 56s | **18m 08s** | ~6.6× |

These are **measured build-stage reports from different commits**, and therefore **not** same-commit cache-vs-control speedups. Golden Wrench confirmed the cached Linux and macOS Debug **Build** steps completed successfully and the control Linux/macOS jobs were cancelled **before build completion**. The two Windows control jobs finished successfully; their cached-mode counterparts also passed, with no Windows ThinLTO change.

The improvement is credible and operationally important: the cache-mode UNIX jobs built their native target inventories in minutes instead of spending hours linking. Boss observed roughly **one hour overall** for the experiment compared with **three-plus hours** of prior CI; this is an owner-observed comparison, not a completed like-for-like whole-matrix wall measurement.

**Remaining attribution gaps:** (1) the same-commit Unix control did not complete, (2) Linux experimental cache mode also explicitly pins LLD in early CMake probes, (3) configure-primed and later warm-cache work must not be labeled fully cold, (4) runner load and available RAM differ between runs, and (5) a complete same-commit successful matrix was not observed at this checkpoint. Use both per-link duration and full job timing before making a production speedup guarantee.

## 5. Correctness results: distinct from performance

At the review checkpoint, the experimentally cached run reported:

| Platform/configuration | Build | Test status / known outcome |
|---|---|---|
| Windows Debug | Passed | Passed (194/194) |
| Windows Release | Passed | Passed (194/194) |
| Ubuntu Release | Passed | **198/199;** `runtime_console_native_keyboard` failed |
| Ubuntu Debug | Passed | **198/199;** `runtime_console_native_keyboard` failed |
| macOS ARM64 Debug | Passed | **195/199;** four native console suites failed |
| macOS ARM64 Release | Passed | **195/199;** same four native console suites failed |
| Ubuntu Sanitizers | Passed | **198/199;** `runtime_console_native_keyboard` failed |

For jobs that failed at CTest, later acceptance-gated steps (including some stack/editor/package checks and the warm relink) did **not** run; a successful build must not be mistaken for those checks passing.

**Ubuntu cross-configuration pattern:** Release, Debug **and Sanitizers** each reported **198/199**, failing `runtime_console_native_keyboard` while the other 198 registered suites passed in each configuration.

**Ubuntu real-PTY keyboard symptom:** `runtime_console_native_keyboard` expected `b'5 0 0 1 1b5b41'` (a complete `ESC [ A` Up Arrow sequence) but received `b'4 0 0 1 1b'` (Escape alone), as documented by Silver Wrench. The test writes key sequences through a real pseudoterminal; `src/runtime/console.cpp` has a 30 ms continuation window for distinguishing lone ESC from multibyte key sequences. Delayed or fragmented PTY delivery is plausible but **unproven**. Main CI #97 Ubuntu Release passed this test; it is a different source revision/run, not a controlled explanation. LLVM code generation, scheduling, real descriptor behavior, and fixture timing have not been ruled out.

**macOS Debug and Release are a separate concern, not merely the Ubuntu key timeout:** both completed native CTest matrices reported the same four failures:

- `runtime_console_native_transfers`: pipe case, `borrowed flags changed by transfers`.
- `runtime_console_native_keyboard`: `native nested mode failed` (not the same reported ESC-vs-Up-Arrow assertion).
- `runtime_console_native_input`: `input changed borrowed descriptors`.
- `stage10_console_aot`: after numerous successful JIT/AOT output cases, a real PTY case failed during terminal-session restoration: `console restore_mode stdin: io_failure; native=5`, with cleanup errors.

These might share a host/PTY/descriptor-root cause, or might be distinct defects. Neither grouping nor causal relationship to ThinLTO caching is established. They require native evidence, not a weakened test.

The control run's Linux/macOS jobs were cancelled without CTest; consequently there is no completed same-commit noncached native regression control for those platforms. The existing main workflow remains authoritative. Passing compilation alone is not Stage 10 Step 42 acceptance.

## 6. Focused failure investigation — without unnecessary full rebuilds

1. Preserve GitHub job logs, `ctest-results.xml`, `Testing/Temporary/LastTest.log`, original expected and actual bytes, and the target's exact compiler flags and output hash. Do **not** change the diagnostics or expected bytes merely to obtain green.
2. On an available **matching built binary**, repeat the Ubuntu native-keyboard test as originally configured:

   ```bash
   ctest --test-dir build/Release -R '^runtime_console_native_keyboard$' \
     --output-on-failure --repeat until-fail:20
   ```

   Run Debug too if available; compare both against an uncached build on **the same commit**. GitHub hosted workspaces are ephemeral; only use a preserved native build or a deliberate new build. Do not assume a job's logs include the executable.
3. If intermittent, record PTY sequence delivery/read fragmentation, timeouts, per-read byte batches, host OS/PTY settings and repeated failures. Do not add sleeps or broaden the 30 ms guard as a first response. If reproducibly cache-specific, compare native object/link identities and emitted behavior; do not dismiss as flakiness.
4. Reproduce the three macOS native `console_*` failures separately, including descriptor flags/ownership and nested mode restoration. The `stage10_console_aot` restoration failure must be checked on actual macOS with exact cleanup/status assertions. A green Linux test does not certify Darwin's borrowing/termios behavior.
5. Preserve verified source snapshots, build logs, JIT/linked-AOT behavior and the original 1 MiB frontend/depth probes. No test suppression, looser formatting, altered sanitizer settings, mandatory OS timezone use, or Git actions without Boss authorization.

## 7. Reproduction and artifact inspection

Use **Actions → Compiler link experiment → Run workflow** and select either `control` or `thinlto-cache` against the **same immutable commit/ref**. Confirm `identity.json` records the same `commit`, `dispatch_sha`, toolchain, target OS/arch and flags; run modes are separate manual dispatches. This report is **not** authorization to launch them while another run is active.

For each configuration inspect the experiment artifact named like `link-experiment-<mode>-<OS>-<arch>-<configuration>-<sha>-<attempt>`:

- `identity.json`: commit/SDK/compiler/runner and workflow digest; host resource data.
- `configure.json` and `configure-trace.jsonl`: CMake flags, ORC probe cost and whether the job-local cache was primed before building.
- `build.json`, `build.log`, `build-links.csv`, `build.ninja_log`, `ninja-targets.txt`: actual build results, link durations and target coverage.
- Cache file/byte totals at configure/build/relink phases: assess reuse rather than equating successful builds with cache hits.
- `ctest-results.xml`, test logs, failed-exit descriptions, and original stack/editor checks: correctness is a separate acceptance gate.
- `relink.json` and relink logs/CSV **only if present after successful tests**: warm representative `voidc` relink, not a substitute for the full build and never infer that skipped relinks passed.

Compare like-for-like build inventories, `-j3`, source SHA, SDKs, configs and complete test suites. Separate (a) empty cache at job setup, (b) **ORC-probe-primed** cache at build start, and (c) warm relink after acceptance.

## 8. Reversal and production-adoption criteria

**Rollback / disable:** stop manually dispatching the isolated experiment; the normal workflow remains unchanged. Boss may remove or disable `.github/workflows/ci-link-experiment.yml` through a separately authorized change. The job-local temporary caches disappear with runners; no persistent cache need be purged. If caching is later promoted into main CI, restoring the original link flags and removing cache setup is the minimal rollback. Do not mutate source or rebuild policy to compensate for an experimental failure.

**Before production CI adoption, require all of:**

1. Clear ownership/authorization from Boss after independent Golden Wrench review.
2. A comparable **same-commit** native control vs cache run, with actual Linux/macOS completion, stable toolchains/inventories and separate early-LLD-probe versus ThinLTO cache effects.
3. All seven job configurations and original registered CTest inventories green on the optimized workflow, plus the real 1 MiB gates, independent AOT/JIT outcomes, editor tests, terminal/GC/cleanup tests and packaging; especially the native keyboard and macOS descriptor/mode failures.
4. Recorded configure/build/link and wall timings, cache-before/after counts, space usage, host memory/resource effects, and at least one repeated comparison to bound CI-host noise.
5. A bounded, reversible change within the existing main CMake/CI infrastructure. Preserve LLVM 19.1.7, ABI, sanitizer, timeout and test contracts; do not introduce a new build framework or cross-run persistent cache without evidence.
6. Do not label Step 42 or Stage 10 locked until their separate native/manual gates and Boss release decision are met.

**Golden Wrench disposition:** **Performance experiment useful and worth preserving; main-CI promotion on hold pending correctness and controlled evidence.** No source implementation or hosted CI action is authorized by this documentation.

**THREE CLANKERS. ONE VOID. STRONG BOLTS. NO GHOST BOLTS!**
