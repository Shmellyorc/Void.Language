# Stage 8 Sprint 1 local acceptance

20 October 2026. **IMPLEMENTED AND LOCALLY VERIFIED; ready for Wrench review.
Hosted acceptance is pending.** Stage 7's owner-supplied hosted lock is
`f704d75ffa37bfd0e587ec2b1513054e07a8a4d8`, CI #81 attempt 2, seven jobs,
980/980 suites. No new hosted run, Git mutation or publication is claimed.
Sprints 2–6 remain future work.

Contract: [STAGE8_DISCOVERY.md](../STAGE8_DISCOVERY.md).
[Machine-readable results](results.json) and [changed source hashes](source-hashes.json)
retain the local evidence; raw logs are in `build/stage8-acceptance/`.

## Consolidated verification

| Configuration | Result | Time |
| --- | --- | --- |
| Debug, `build/debug` | 141/141, zero failures | 438.15 s |
| Release, `build/ci-release` | 141/141, zero failures | 166.45 s |
| RelWithDebInfo, strict ASan/UBSan/leaks, `build/ci-sanitizers` | 141/141, zero failures | 373.17 s |

**423/423 consolidated suites passed.** Focused frontend, modules, analysis,
tooling, sessions, protocol and Sprint 7/8 pipe checks passed **8/8**. Separate
1 MiB frontend gates passed Debug **10.79 s**, Release **6.01 s**. After the
consolidated runs, a narrow known-standard-URI Windows drive-letter spelling
repair received final server rebuilds and **5/5 LSP suites in each configuration**
(15/15), retaining strict sanitizer settings. Its Windows-only branch remains a
hosted-platform verification target; Linux execution is not Windows evidence.

Commands from repository root:

```sh
cmake --build build/debug -j4
ctest --test-dir build/debug --output-on-failure -j2
cmake --build build/ci-release -j4
ctest --test-dir build/ci-release --output-on-failure -j2
cmake --build build/ci-sanitizers -j4
ASAN_OPTIONS=detect_leaks=1:halt_on_error=1:exitcode=87 \
UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1:exitcode=86 \
ctest --test-dir build/ci-sanitizers --output-on-failure -j2
bash -c 'ulimit -s 1024; ctest --test-dir build/debug --output-on-failure -R "^frontend$"'
bash -c 'ulimit -s 1024; ctest --test-dir build/ci-release --output-on-failure -R "^frontend$"'
# Final server-only rebuild/query checks, repeated for all three configurations:
cmake --build build/debug --target voidls -j4
ctest --test-dir build/debug -R '^lsp_' --output-on-failure
```

The sanitizer build and tests ran outside ptrace: LeakSanitizer explicitly failed
inside the tracing sandbox during fixture emission. Leak detection, assertions and
halt-on-error stayed enabled. C++ compiler/Runtime instrumentation does not imply
that generated LLVM application code is itself sanitizer-instrumented.

[Engine fixture evidence](discovery-engines.json): distinct same-name standard and
project modules, nested standard internal dependencies and bare quoted project
imports produced **16** in normal/O2 JIT and linked AOT. Debug cache miss/hit
worked; changing only standard source bytes caused a miss and result **21**.
[Existing website gallery evidence](website-examples.json): all **15** published
contracts checked and produced their advertised outputs in normal/O2 native AOT;
all **12 synchronous** examples also passed normal/O2 JIT. The three async examples
used the existing explicit async AOT adapter. No website files changed.

```sh
python3 tools/verify_discovery_engines.py build/debug --output /tmp/discovery-engines.json
python3 tools/verify_website_examples.py build/debug --output /tmp/website-examples.json
```

## Actual editors

- **VS Code Linux 1.141.0**, language client 10.1.2, Node 24.19.0: all three
  development profiles passed. Actual `type` input and selected-suggestion
  acceptance verified every requested prefix, including nested quoted file closure.
  API docs/navigation and existing dirty/versioned edits, fixed selection across
  main creation, explicit override, restart, trust, failure and deactivation checks
  passed. Chromium required the available desktop outside the tracing sandbox.
- **Neovim 0.13 development**, **portable 0.12.6**, and **actual Vim 9.2** with
  pinned yegappan/lsp `c741b2f1afb42b9f267824dac4049674908f4a41`: shipped-adapter
  RPC/edit, source docs/navigation and lifecycle checks passed. Automatic and
  explicit entry profiles passed on installed Neovim/Vim.
- Separate **real terminal typing and popup acceptance** passed all five prefixes
  in both Neovim versions and Vim. Neovim used normal `menu,menuone,noinsert`
  presentation; Vim used `menuone,noinsert` and selected the first item before
  accepting. These tests exercise actual editor event loops, not manual completion
  requests. Neovim's test-control UNIX socket required an outside-sandbox run.

[Adapter evidence](editors.json), [explicit selection](editors-explicit.json),
[portable Neovim](neovim-stable.json), and
[automatic terminal menus and exact accepted text](automatic-terminal-completion.json).

```sh
export PATH=/home/shmellyorc/.nvm/versions/node/v24.19.0/bin:$PATH
npm --prefix editors/vscode test
VOIDLS_PATH="$PWD/build/debug/voidls" VSCODE_EXECUTABLE_PATH=/usr/share/code/code \
  npm --prefix editors/vscode run test:host
python3 tests/editors/run.py --server build/debug/voidls --nvim /usr/bin/nvim \
  --vim /usr/bin/vim.basic --vim-client /path/to/pinned/lsp --output /tmp/editors.json
# Repeat with --explicit-entry and with portable Neovim.
python3 tests/editors/neovim_automatic.py --server build/debug/voidls \
  --nvim /usr/bin/nvim --output /tmp/nvim-automatic.json
python3 tests/editors/vim_automatic.py --server build/debug/voidls \
  --vim /usr/bin/vim.basic --vim-client /path/to/pinned/lsp --output /tmp/vim-automatic.json
```

## Metadata measurements

Seven observations per workload; first request is reported separately, without
claiming a cold OS page cache. Maximum is the sample tail, not an established p95.
Parallel build/test activity may affect timings; no universal latency SLA is locked.

| Siblings | Compiler metadata median / max, ms | LSP median / max, ms | Results / compact result bytes |
| --- | --- | --- | --- |
| 10 | 1.24 / 1.51 | 0.94 / 1.13 | 10 / 2,032 |
| 1,000 | 16.86 / 17.23 | 21.28 / 31.51 | 256, incomplete / 51,743 |
| 10,000 | 39.60 / 41.01 | 42.75 / 44.52 | 256, incomplete / 51,743 |

The compiler-only workload adds 1,000 overlays and 40 irrelevant descendant
levels, and deliberately invalid candidate bodies that must never be parsed.
Independent empty-project semantic medians were 0.07–0.08 ms. The pipe workload
has no extra overlays; its separate no-op barrier median was 0.07–0.10 ms.
These are different workload components, not subtractable latency measurements.
Compiler probe process peak RSS was 63,824 KiB including its loaded LLVM library
and fixture work; LSP process peaks were 9,248–14,364 KiB. Specific matches beyond
the cap remained discoverable. Directory menus do not recursively scan or compile
candidate bodies. The initial Debug overlay-heavy probe exposed repeated overlay
scans; query-local virtual-child maps reduced the 10,000-sibling observed median
from about 1.1 s to 137 ms, before the final Release measurements above.

[Compiler data](metadata-release.jsonl), [process memory](metadata-release-memory.json),
[protocol data](protocol-release.jsonl). Reproduction uses the matching library:

```sh
clang++-19 -std=c++20 -O2 -Iinclude tools/measure_discovery.cpp \
  build/ci-release/libvoid_core.a $(llvm-config-19 --ldflags --libs --system-libs support) \
  -o /tmp/void-discovery-benchmark
/tmp/void-discovery-benchmark /tmp/new-nonexistent-probe-directory
python3 tools/measure_discovery.py build/ci-release/voidls
```

## Repairs, review choices and limits

The old malformed `use bare` fixture became legal syntax; recovery now uses
`use bare/`. Old negative bare quoted import expectations were updated to the
approved spelling while retaining assertions. Focused tests cover domain/cycle
identity, aliases, private/internal visibility, root constant isolation, source
navigation, docs/parameters, overlays/tombstones/virtual parents, unsafe paths,
symlinks, hard-link aliases despite diagnostic-only overlay filenames, case checks,
BOM/UTF-16 positions, portable spaces, quotes/suffixes/aliases/comments, source-root
failures, bounded project selection and readonly standard edit guards. Existing
cache, rename, formatting, GC, native, async and reload suites remain green.
Relative CMake source-root configuration is explicitly rejected; the negative
configuration probe failed at the intended absolute-root validation.

Wrench should review the recorded concrete identity, configuration, documentation
and selection policies. Remaining **unverified** combinations: VSCodium,
Windows/macOS native/client execution, minimum client versions, new installed VSIX
profiles and clean-machine portability. Relocated installations need their matching
absolute `VOID_STDLIB_ROOT`. No official standard-library catalog, optional-main
execution or public distribution is delivered. No known failing local test remains.
Boss alone controls commit/push, hosted seven-job acceptance and the Sprint 1 lock.
