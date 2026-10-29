# LLVM 23.1.4 local acceptance

**Locally complete — ready for independent Golden Wrench review.**

Local acceptance covers the migrated working tree, including retained failures and
focused corrections. It is not a seven-job hosted lock or native Windows/macOS
acceptance. Compiled compiler/Runtime inputs stayed unchanged throughout final
validation; the last correction only reuses native manifest flags in two tests.

Reference and unchanged HEAD: `db808efe0fa191ed25733513cc9ebcebbadc3bc2`.
The [engineering history](IMPLEMENTATION.md) retains first failures and demonstrated
repairs. [Compatibility](COMPATIBILITY.md) records API changes and platform limits.
Original nine local logging documents, root README and historical evidence remain
preserved. No staging, commit, branch, push, hosted CI or publication is performed.

## Toolchain and builds

- Official complete Linux x64 LLVM/Clang/LLD **23.1.4**, commit
  `7a941dfb2877a0b8cf30323fefa85ad019e0b0f5`, installed beside LLVM19.
- Archive SHA-256:
  `25d814fda50d5fb0084b42be09605d8b1595d49d5e2e03a3345d7e112ff6f9f3`.
- Fresh all-target builds complete in `build/llvm23-release`, `build/llvm23-debug`
  (optional LLVM research ON), and `build/llvm23-strict` (ASan/UBSan ON).
  Each actual CMake ORC executable probe links and runs. Final SDK-facing link
  inputs select only LLVM23; identities are in `evidence/accepted-binary-identity.json`.
- C++20, matching no-RTTI, project exceptions and host libstdc++ ABI retained.
  The supplied no-RTTI SDK cannot provide UBSan vptr coverage. Prebuilt SDK code
  and ordinary generated JIT instructions are not claimed sanitizer-instrumented.
- Installer tests verify actual Linux cache reuse, published three-platform asset
  selection, rejected unverified cache and rejected corrupt download before
  extraction. The actual cached SDK installer entrypoint passes. Linux-hosted
  COFF/Mach-O LLD version checks pass; these are not native-platform execution.
- Shared `finish` implementation is byte-identical to HEAD. The recorded standard
  no-target-machine O2 recipe retains DSE and pre/post verification.

## Consolidated profile results

| Profile | Selection | Result |
|---|---:|---|
| Initial Release engineering run | 368 | 348 passed, 20 failed; original output retained |
| Integrated Release | 368 | 367 passed, numeric fixture discovery failed; original output retained |
| Corrected numeric integration | 1 | **1/1 passed**, 13.00 s; effective Release **368/368** |
| Debug including optional research | 381 | **381/381 passed**, 3846.39 s |
| Strict ASan/UBSan/leaks | 358 | 356 passed / 2 test-link failures in 3434.55 s; corrected **2/2** in 92.92 s; effective **358/358** |
| Release frontend, hard 1 MiB | 1 | **1/1 passed**, 3.79 s |
| Debug frontend, hard 1 MiB | 1 | **1/1 passed**, 9.29 s |

Final profile commands run CTest with `--output-on-failure --output-junit` and two
independent test processes. Strict inherits:

```text
ASAN_OPTIONS=detect_leaks=1:halt_on_error=1:exitcode=87
UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1:exitcode=86
LSAN_OPTIONS=exitcode=87
```

The separate frontend commands set both soft and hard `RLIMIT_STACK` to 1,048,576
bytes before CTest. Many existing module execution/stack tests independently impose
actual hard limits; a test named `stack` is not substituted for observing the limit.
No new test or negative assertion is disabled. The unchanged sanitizer profile
omits ten duplicate stack selections; Debug/Release retain all ten. Exact names
and all three command inventories are in `evidence/test-inventory-comparison.json`
and `test-inventory-{release,debug,strict}.json`. Existing source/Runtime/GC/async/native/
reload/cache/LSP and independently linked AOT selections are retained in full.

## Additional verified boundaries

| Check | Actual outcome / receipt |
|---|---|
| Focused hard-stack normal/O2 JIT, native build, separate native execution | **6/6**, `focused-execution-corrected.json` |
| Shared crash/timeout/source preservation | Existing synthetic assertions plus inherited-env/empty-env regression pass, `crash-recorder-regression.txt` |
| Math incident metadata forwarding | Three synthetic preservation incidents pass, `math-preservation-corrected.txt` |
| Entry-pruning metadata coherence | Focused cache test passes normal/O2 and preserves unrelated pruning, `debug-cache-corrected.txt` |
| Cross-SDK persistent objects | 19 miss/hit, 23 miss/hit, 19 hit; original object bytes retained, `cross-version-cache-native.json` |
| Stale native manifest | Old manifest rejected, owner output sentinel intact, same receipt |
| Vim + Neovim adapters | Automatic entry and explicit override both pass, `editors-{automatic-corrected,explicit}.json` |
| Actual insert-mode completion | Both PTYs accept all five import prefixes, `neovim-terminal.json`, `vim-terminal.json`; Vim actual signature popup transitions pass |
| Extension TypeScript + unit suite | Build/typecheck and **4/4** tests pass, `extension-unit.txt`, `extension-build-final.txt` |
| Actual VS Code development host | Trusted, restricted-trust and multi-root pass, `extension-host.txt` |
| Private native archive / VSIX | Dependency audit, LLVM notice, extracted-server handshake with OS-only PATH pass, `tooling-package-final.txt`; executable payloads match installed-host-tested package, `package-readme-reconciliation.json` |
| Installed VSIX / bundled server | Actual isolated installation and all three host modes pass, `extension-installed-host.txt` |
| Active tooling source/config audit | 20 edited Python files parse; five active workflows use 23.1.4 with existing matrix, `active-tooling-audit-final.json` |

Actual local editor versions: VS Code **1.143.0**, Vim **9.2**, Neovim
**0.13.0-dev-1410+g510f61555b**; pinned Vim LSP client commit
`c741b2f1afb42b9f267824dac4049674908f4a41`. Provider/host tests are distinguished
from visual UI observation: no new CDP popup capture is claimed. VSCodium is not
executed. Editor fixtures now include their existing `error` module dependency;
no second parser or semantic engine is added.

## Original failures and corrected scope

First Release failures comprise 16 JSON tests stopped by the shared Python
`env=None` recorder bug, two scalar-list diagnostics missing `index` context,
Math's incident adapter missing keyword metadata forwarding, and custom-equality
metadata pruned from entry emission. Source comparisons reproduce both compiler
issues with preserved LLVM19 as well as LLVM23; they are not blamed on the upgrade.
Strict channel AOT links also guessed sanitizer flags from a build-directory
substring. Both now consume the existing native manifest flags in one support
helper; original undefined-sanitizer-symbol link errors remain retained. This
test-only correction does not alter compiled compiler/Runtime inputs. The full
Debug result precedes this last helper correction; Linux Debug/Release flags
remain equivalent, and focused strict validation covers the corrected calls.
Later console assertions exposed a private cold-cache fixture already warmed by
ordinary automatic caching. Repeated numeric discovery also included the `.void`
cache directory as a source. These test setups are corrected without reducing
actual cases or changing production cache semantics. Original stdout/stderr,
JUnit and incident artifacts remain retained.

The optional storage certifier matches LLVM23 stock target-aware library-analysis
setup. New O2 cloned homes remain unproved and reject with exact reasons, preserving
owner bytes. This changed successful certification domain is explicit in
[compatibility](COMPATIBILITY.md); valid source execution remains independently
required. No production ownership inference or optimizer suppression is introduced.

## Bounded measurements and historical incidents

The [reproducible bounded measurements](evidence/metrics/README.md) complete
**20/20** checked child executions: five representative sources × normal/O2 ×
LLVM19/23, with three separately timed invocations per compilation. All children
use actual hard/soft 1 MiB stacks and ASLR remains enabled. Runs are serial after
acceptance is idle. Both versions use the same current source/stdlib bytes; the
preserved LLVM19 versus migrated LLVM23 Release libraries/compiler differ.
These are full-module embedding observations without persistent caching, not
warm entry-pruned CLI latency or native hosted build-time measurements.

Compilation below sums analysis, typed lowering, JIT setup and LLVM/ORC compilation
before invocation. Execution is the median of three in-process calls. There is
one compilation sample per point, on PikaOS 4/x86-64, Ryzen 7 1700X, glibc 2.44.
No confidence interval, cross-platform determinism or LLVM-only causal attribution
is claimed. Raw phase/CPU/RSS data and exact commands are in
[results](evidence/metrics/results.json), [comparison CSV](evidence/metrics/comparison.csv)
and [environment/source identities](evidence/metrics/environment.json).

| Workload | Mode | Compile 19 → 23 (s) | Change | Execution 19 → 23 (ms) | Change |
|---|---|---:|---:|---:|---:|
| numeric | normal | 0.028 → 0.025 | -10.5% | 41.509 → 42.108 | +1.4% |
| numeric | O2 | 0.038 → 0.028 | -26.5% | 5.743 → 5.448 | -5.1% |
| json | normal | 3.082 → 2.559 | -17.0% | 73.118 → 69.788 | -4.6% |
| json | O2 | 3.779 → 3.026 | -19.9% | 72.231 → 68.733 | -4.8% |
| async | normal | 2.574 → 2.043 | -20.6% | 2.383 → 2.002 | -16.0% |
| async | O2 | 3.431 → 2.572 | -25.0% | 1.942 → 2.002 | +3.1% |
| logging | normal | 7.622 → 6.089 | -20.1% | 1.881 → 1.845 | -1.9% |
| logging | O2 | 10.033 → 7.721 | -23.0% | 1.816 → 1.912 | +5.3% |
| text | normal | 0.777 → 0.626 | -19.4% | 2083.420 → 2154.530 | +3.4% |
| text | O2 | 0.933 → 0.724 | -22.4% | 2188.830 → 2091.900 | -4.4% |

Every measured compilation point is lower by **10.5–26.5%**. Execution changes
range from **−16.0% to +5.3%**; small changes, especially millisecond IO/logging
cases, are observations rather than demonstrated regressions/improvements. The
normal text point is +3.4%; optimized logging +5.3%; optimized async +3.1%; normal
numeric +1.4%. They are retained alongside improvements. Text/codec execution is
about two seconds for 65,536 UTF-8 input bytes in this existing workload on both
versions; this migration does not implement unrelated text/logging optimization.

Distribution size increases are material: Release `voidc` **+27.94%** and `voidls`
**+15.30%**, with exact byte counts in `artifact-sizes.json`. These are configured
local artifacts, without additional stripping or ABI/optimization changes. Five
sampled SDK library members are native ELF in LLVM23 and bitcode in LLVM19; that
is not an exhaustive archive classification or proof of hosted linking speed.

All **3/3 bounded DSE-associated checks pass**:

| Input / phase | Verified local outcome |
|---|---|
| Retained Stage11 preopt IR, restored `/main.void` identifier | Parse + pre/post verified standard no-TM O2: 0.299 s |
| Captured logging bitcode, restored `/performance.void` identifier | Parse + pre/post verified standard no-TM O2: 2.236 s |
| Source-associated logging fixture | LLVM/ORC compilation 7.048 s; three invocations each checksum 12,800; median 8.107 ms |

Exact inputs, source claims, hashes and output are in
[DSE results](evidence/metrics/dse-results.json). No new SIGSEGV, core or sanitizer
finding occurs in these bounded checks. These are retained diagnostic inputs and
a source-associated fixture, not the original crashing optimizer heap/pass state.
The original LLVM19 executable/core/preopt/source identities are preserved in
`dse-preservation-before.json` and rechecked by the final inventory.
**The original cause and any claimed repair remain unproven.** No unlimited crash
campaign, ptrace, DSE suppression, optimizer switch or production phase instrument
is introduced.

## Exact review inputs

[Final inventory](evidence/final-inventory.json) records all **58 migration-modified**
tracked paths with before/after SHA-256, categorized new documentation/acceptance
receipts, all nine original dirty-document hashes, protected root README, unchanged
HEAD/index, all-other-original-tracked-file preservation and original incident
identities. Initial tracked snapshot covers 22,157 paths. The executable/test input
freeze is `integration-source-freeze-acceptance-3.json`; the three later Python
changes are exclusively the corrected channel link harness. Debug/Release link
flags remain equivalent in Linux; strict reruns exercise the corrected native
links under fatal sanitizers. No compiled source changed after the consolidated
profiles began. Final binary/archive/package identities are in
`accepted-binary-identity.json`. All original failed logs/XML/preparation errors
remain alongside the successful focused corrections.

## Remaining acceptance boundaries

Native Windows Debug/Release and macOS ARM64 Debug/Release execution, SDK ABI,
Unicode/percent/deep-path cases, stop-token probe, native editor integration and
hosted seven-job acceptance require later Boss-controlled execution. Workflow
migration is not evidence that those jobs passed. Private Linux artifacts retain
host dependency requirements and are not a public portable distribution.
Independent Golden Wrench source/architecture review is the next gate; local
engineering stops here. No release lock or historical incident closure is inferred.
