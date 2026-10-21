# Stage 7 local acceptance — 20 October 2026

**Superseding owner evidence (20 October 2026): Stage 7 HOSTED LOCKED** at
`f704d75ffa37bfd0e587ec2b1513054e07a8a4d8`, CI #81 attempt 2, seven successful
jobs and 980/980 suites. This is owner-supplied hosted evidence, not a new local
or independent hosted run. Pending-lock wording below is historical.
Current work is [Stage 8 Sprint 1](../STAGE8_DISCOVERY.md).


Production engineering is locally complete for the bounded functionality in
[STAGE7_IMPLEMENTATION.md](../STAGE7_IMPLEMENTATION.md). Stage 7 is **not locked**.
Stop for Wrench review and Boss-controlled commit, push and seven-job hosted CI.
No Git history, branch, remote, hosted workflow or public publication was changed.
The original Stage 6 HEAD remains `4139297f92c195782f59a28546444e6b3bec4ad8`.

## Consolidated native acceptance

PikaOS Linux x86-64, glibc 2.44, Ryzen 7 1700X; LLVM/Clang/LLD **19.1.7**.
All final compiler/server source changes preceded these consolidated runs.

| Configuration | Result | CTest wall time |
| --- | --- | --- |
| Debug, `build/order1/production` | 140/140 passed, 0 failed | 427.92 s |
| Release, `build/ci-release` | 140/140 passed, 0 failed | 165.36 s |
| RelWithDebInfo, strict ASan/UBSan/leaks, `build/ci-sanitizers` | 140/140 passed, 0 failed | 365.19 s |

**420/420** consolidated suites passed. Existing native/runtime/GC/async/reload/
cache/compiler/ABI regressions remain in the matrix. The additional `lsp_stage7`
suite covers recovery, UTF-16, bindings, rename/format safety, refresh, batching,
cancellation and shutdown. Tooling tests compare actual normal/O2 JIT behavior
before/after formatting; they also cover exact comments, CRLF and idempotence.

Executed build/test commands (from repository root):

```sh
cmake --build build/order1/production -j2
ctest --test-dir build/order1/production --output-on-failure -j2
cmake --build build/ci-release -j2
ctest --test-dir build/ci-release --output-on-failure -j2
cmake --build build/ci-sanitizers -j2
ASAN_OPTIONS=detect_leaks=1:halt_on_error=1:exitcode=87 \
UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1:exitcode=86 \
ctest --test-dir build/ci-sanitizers --output-on-failure -j2
bash -c 'ulimit -s 1024; ctest --test-dir build/order1/production --output-on-failure -R "^frontend$"'
bash -c 'ulimit -s 1024; ctest --test-dir build/ci-release --output-on-failure -R "^frontend$"'
```

Strict leak tests ran outside the tracing sandbox because LSan cannot run under
ptrace. The separate 1 MiB frontend gates also passed: Debug 10.60 s, Release
4.55 s. The permanent semantic-depth regression now accepts legal 16/64/96/124
receiver chains and retains original excessive-depth rejection assertions.

A new cold editor-probe temporary initially enlarged recursive semantic frames;
it was moved behind noinline query boundaries. A freshly extracted HEAD also
reproduced pre-existing capability analysis failure at 96 calls and lowering
failure at 64 calls on a 1 MiB stack. Narrow noinline capability/lowering case
boundaries repair those frames without lowering guards or changing evaluation
order. [Stack evidence](semantic-stack.json) records ordinary rejection beyond
the accepted range. Four freshly built HEAD-versus-current native compatibility/
constant contracts remain byte-identical, including generic, advanced and async
examples: [hashes and provenance](native-contracts.json).

## Actual editor execution

| Client | Executed combination | Result |
| --- | --- | --- |
| VS Code | Linux 1.141.0, pinned language client 10.1.2 | Development extension: all 3 profiles passed |
| VS Code installed artifact | Linux 1.141.0, actual final platform VSIX, bundled Release server | All 3 profiles passed |
| Installed Neovim | `NVIM v0.13.0-dev-1410+g510f61555b`, Release, LuaJIT 2.1.1787165859 | Shipped adapter passed against final Release server |
| Portable Neovim | 0.12.6, built-in LSP | Shipped adapter passed against Debug server |
| Actual Vim | `/usr/bin/vim.basic`, 9.2 patches 1–858, yegappan/lsp `c741b2f1afb42b9f267824dac4049674908f4a41` | Shipped adapter passed against final Release server |

The installed `vim` alternative now resolves to Neovim. Tests explicitly select
`vim.basic` to exercise actual Vim; no alternatives or user configurations were
changed. Editors use disposable fixtures/config/state; VSIX installation uses a
disposable extension directory. No system-wide extension was installed.

VS Code trusted profiles exercise unsaved diagnostics/repair, completion selection,
incomplete-call signatures, hover, symbols, navigation, cross-file rename,
format/idempotence, refresh, close/reopen/restart/configuration/failure and child
cleanup. Separate restricted-trust and multi-root profiles refuse native startup.
The installed artifact uses bundled discovery with the same assertions.
Neovim additionally tests versioned edit preflight, null-version dirty targets,
stale formatting and no orphan native server. Vim exercises completion/signatures,
hover/symbols/navigation, dirty repair, guarded formatting, refresh/restart/stop;
rename remains explicitly disabled. Exact outcomes:
[installed development Neovim and Vim](editors-linux-dev.json),
[portable Neovim and Vim](editors-linux-stable.json).

Executed extension/editor commands (Node build runtime 24.19.0):

```sh
export PATH=/home/shmellyorc/.nvm/versions/node/v24.19.0/bin:$PATH
npm --prefix editors/vscode test
export VOIDLS_PATH="$PWD/build/ci-release/voidls"
export VSCODE_EXECUTABLE_PATH=/usr/share/code/code
npm --prefix editors/vscode run test:host
VOID_TEST_VSIX="$PWD/build/stage7-artifacts/void-language-linux-x64.vsix" \
  npm --prefix editors/vscode run test:host
python3 tests/editors/run.py --server build/ci-release/voidls \
  --nvim /usr/bin/nvim --vim /usr/bin/vim.basic \
  --vim-client /tmp/void-stage7-clients/lsp-c741b2f1afb42b9f267824dac4049674908f4a41 \
  --output docs/stage7/editors-linux-dev.json
```

The portable run used the same editor harness with
`--nvim /tmp/void-stage7-clients/nvim-linux-x86_64/bin/nvim`,
`--server build/order1/production/voidls` and output `editors-linux-stable.json`.
GUI execution used the available desktop outside the tracing sandbox. Typecheck,
build and extension unit tests passed. Earlier harness failures (URI percent-case,
ambiguous fixture search and installed-extension per-extension API identity) were
corrected without removing production safety assertions.

## Before/after measurements

Same research fixture generator, Release configuration, host and seven repetitions;
CLI includes process startup and IR lowering. Times are milliseconds; each `p95`
from seven observations is the largest sample, not a statistically established
service percentile. Definition has 70 samples. No claim of overall compiler speedup
or editor keystroke-to-screen latency follows from pipe timings.

| Graph | CLI median before → after | Startup median before → after | Valid edit median before → after (after p95) | Idle RSS KiB before → after |
| --- | --- | --- | --- | --- |
| small (3 modules) | 7.53 → 7.36 | 4.15 → 3.69 | 0.65 → 0.85 (1.08) | 8800 → 9520 |
| medium (17 modules) | 226.50 → 233.22 | 48.49 → 51.17 | 40.29 → 44.28 (45.12) | 28232 → 38948 |
| large (33 modules) | 3173.39 → 3263.47 | 246.75 → 262.59 | 233.93 → 271.76 (292.14) | 94596 → 129584 |

The large queued 20-edit burst fell from **4800.57 ms / 660 diagnostic
notifications to 277.77 ms / 33 notifications** (one final graph publication;
about 17.3× shorter in this run). Single-edit analysis and retained tooling metadata
have measurable costs: large valid edit +16.2%, idle RSS +37.0%. This sprint fixes
the demonstrated backlog, not incremental analysis. Definition median remains
0.067 ms on the large graph (before 0.078 ms).

Query medians / sample p95, including real graph preflight where required:

| Graph | Completion | Signature help | Hover | Rename | Formatting |
| --- | --- | --- | --- | --- | --- |
| small | 1.15 / 1.35 | 0.77 / 1.33 | 0.09 / 0.12 | 1.12 / 1.46 | 0.79 / 0.84 |
| medium | 29.08 / 29.48 | 29.47 / 29.77 | 0.11 / 0.24 | 53.16 / 56.92 | 40.16 / 41.58 |
| large | 191.78 / 201.76 | 186.72 / 189.19 | 0.11 / 0.22 | 430.02 / 432.48 | 253.96 / 257.44 |

Completion uses an incomplete `return m0.` line; other queries use the restored
valid graph. Each query group starts after its overlay synchronization barrier;
synchronization time is excluded. No rename/format edits are applied during these
measurements. Initial measurement used an existing call-name token whose recovery
is conservatively suppressed; the workload was corrected to measure the supported
incomplete member context rather than accepting an empty result.

Raw reproducible results: [before](../research/stage7/baseline-linux-release.json),
[after](after-linux-release.json), [real examples](after-examples-linux-release.json),
[query latency](query-latency-linux-release.json). Commands:

```sh
python3 docs/research/stage7/measure_baseline.py --build build/ci-release
python3 docs/research/stage7/measure_baseline.py --build build/ci-release --examples-only
python3 tools/measure_tooling.py --build build/ci-release
```

The historical measurement script's `baseline` field identifies the source HEAD;
`binaries.*.sha256` identifies the measured uncommitted candidate executable.

## Installation and artifacts

Executed local packaging and native install:

```sh
python3 tools/package_tooling.py --server build/ci-release/voidls \
  --output build/stage7-artifacts --target linux-x64 --vsix
cmake --install build/ci-release --prefix "$PWD/build/stage7-install" --component tooling
PATH=/usr/bin:/bin python3 tools/lsp_demo.py build/stage7-install/bin/voidls
```

The package script extracted the native archive and performed an actual LSP
handshake with the SDK path removed. The installed CMake server passed the real
LSP demo. The final VSIX was installed and exercised in the real editor above.
Native archive, CMake installation, VSIX server and final Release `voidls` bytes
are identical. Artifact sizes/hashes and aggregate results are recorded in
[acceptance.json](acceptance.json).

Local deliverables:

- `build/stage7-artifacts/void-tooling-linux-x64.tar.gz` — native server and Vim/Neovim runtime trees.
- `build/stage7-artifacts/void-language-linux-x64.vsix` — platform VSIX with bundled native server.
- `build/stage7-acceptance/` — copied final build, CTest, 1 MiB, editor, installation and packaging logs (ignored build output).

Archives contain LLVM notices, dependency audit and private-acceptance warning.
They require matching host libraries (including C/C++ runtime, zlib/zstd and Z3);
no clean-machine/self-contained portability claim is made. Public licensing,
redistribution obligations, signing and Marketplace/Open VSX policy remain Boss
release decisions. Repository remains UNLICENSED. Nothing was published.

## Remaining boundaries and review

No known failing final local acceptance test remains. No unresolved architecture
choice blocks the implemented subset. Wrench should review compiler probe isolation,
case-frame changes, rename equivalence checks, version safety and transport ordering.

VSCodium, Windows/macOS execution, research minimum editor versions (Neovim 0.11.5,
Vim 9.1), and clean-machine distribution are **unverified**. The extension declares
VS Code ^1.141.0, matching the actual tested release; a successful run does not
certify future editor releases or all development Neovim builds.

Recovery is bounded, not general error-tolerant compilation. Existing call-callee
replacement, invalid enclosing context and some readonly method contexts can return
no completion; signatures require a resolved supported callable. Rename excludes
types, aliases, fields, generic/method/property/interface/virtual families and
captured bindings. Formatting is fixed four-space and conservative. Semantic tokens,
general quick fixes, arbitrary callable signatures, incremental compilation,
multiple entry graphs and general package discovery remain outside this subset.
See the implementation contract for precise client and filesystem guarantees.

Local acceptance does not replace the owner's seven hosted cross-platform jobs.
**Ready for Wrench review; Stage 7 is not locked.**
