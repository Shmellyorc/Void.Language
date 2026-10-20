# Stage 7 research evidence

20 October 2026. Companion to [STAGE7_ROADMAP.md](../../STAGE7_ROADMAP.md).
These are bounded baseline experiments, not production Stage 7 implementations
or a new test framework. No editor profile was installed/modified permanently.

## Baseline and environment

- Local HEAD: `4139297f92c195782f59a28546444e6b3bec4ad8`; worktree was clean
  before research. Boss reports Stage 6 CI #78, 973/973; not independently rerun.
- Linux x64, AMD Ryzen 7 1700X. Release CMake uses `/usr/bin/clang++-19`,
  `-O3 -DNDEBUG`, `/usr/lib/llvm-19/lib/cmake/llvm`, pinned LLVM 19.1.7.
- Exact host, CPU count, executable hashes and raw samples are in the JSON files.
  Source/benchmark fixtures were disposable. Measurements used the existing real
  `tools/lsp_client.py`; its barrier waits through all preceding server output.
- Build/packaging Node: `/home/shmellyorc/.nvm/versions/node/v24.19.0/bin/node`.
  Real VS Code 1.141.0 reports extension-host Node 24.21.0.
- Initial generated `f32`/`f64` names were correctly rejected as reserved; the
  fixture now uses `fn0` etc. No compiler defect was inferred from this.

## Reproduced checks

| Check | Actual local result |
| --- | --- |
| `cmake --build build/ci-release --target voidls voidc -j2` | Passed; measurement binaries match current source build |
| `ctest --test-dir build/ci-release -R '^(tooling\|sessions\|lsp_positions\|lsp_protocol\|lsp_demo\|stage6_integrated)$' --output-on-failure` | 6/6 passed, 0 failed, 6.54 s |
| `npm --prefix editors/vscode test` with Node 24.19.0 on PATH | Typecheck/build passed; Node runner reported 1 test-file passed, 0 failed |
| `npm --prefix editors/vscode run package` | Passed: ignored `dist/void-language-0.1.0.vsix`, 8 files, 145.72 KB; existing bundle-size warning only |
| Existing `npm --prefix editors/vscode run test:host` with installed editor and Release server | Trusted, untrusted and multi-root profiles all passed. Trusted profile checks real diagnostics/ranges, unsaved repair, navigation, stale suppression, close/reopen, restart/configuration, failure/deactivation and child cleanup. Other profiles spawn no native server. |
| `measure_baseline.py` | All assertion-backed generated workloads passed; seven samples and 70 definition requests each, invalid edits/repair and stale-query suppression included |
| `measure_baseline.py --examples-only` | Seven CLI/server runs each for analysis, generics, advanced and async examples passed |
| Real editor timing probe | Seven error/repair cycles and 70 navigation commands passed; results in `baseline-vscode-linux.json` |
| Official portable Neovim 0.12.6 | Attach/definition/unsaved error/stale suppression/repair/references/stop passed |
| Installed Vim 9.2, patches 1–858 + pinned yegappan/lsp | Attach/definition/unsaved error/repair/restored definition/stop passed |

These baseline checks do not exercise unimplemented completion, hover, signature
help, rename or formatting. New Neovim/Vim probes cover a two-file scalar project,
not all language/editor behavior. VSCodium, advertised minimum editor versions,
Windows/macOS new-client tests and portable native bundles were not executed.
No full regression/sanitizer matrix was warranted for documentation/research
scripts; that remains Stage 7 implementation acceptance.

## Reproduce compiler/server measurements

From the repository root:

```sh
python3 docs/research/stage7/measure_baseline.py --build build/ci-release
python3 docs/research/stage7/measure_baseline.py --build build/ci-release --examples-only
```

`baseline-linux-release.json` and `baseline-examples-linux-release.json` contain
the captured runs. The script currently uses Linux `/proc` for CPU/RSS evidence;
adapt that observation for other OSes instead of claiming portability. It writes
JSON to stdout and cleans generated source directories. Startup is spawn through
initialize, initial diagnostics and barrier. Edit timing includes full graph
analysis and protocol output. CLI check includes typed IR lowering. With seven
samples, nearest-rank p95 is the largest observation; it is not a robust population
tail estimate. No caches were flushed and no cold-start distribution is claimed.

Each generated module has 5, 50 or 100 simple functions; entry imports 2, 16 or
32 modules respectively and calls one function from each. Remaining functions
are still validated. A 20-edit burst counts 3/17/33 diagnostic publications per
analysis: 60/340/660 total. The large burst took 4800.57 ms. This demonstrates
repeated full-graph work, not a benchmark of application runtime or native workers.

## Reproduce real editor timing

The extension must already be built with its locked dependencies. The runner uses
the existing `@vscode/test-electron` and a temporary workspace/user/extension
directory, then cleans them. It overwrites `baseline-vscode-linux.json`.

```sh
/home/shmellyorc/.nvm/versions/node/v24.19.0/bin/node \
  docs/research/stage7/run_editor_probe.cjs \
  /usr/share/code/code "$PWD/build/ci-release/voidls"
```

The probe listens for actual VS Code diagnostic events; it does not poll at a
50 ms interval. Activation timing excludes GUI process startup. The extension
may activate on document open before the explicit `activate()` call; the timer
starts before opening. Absolute timings include editor/client scheduling. The
250 ms client pending-change delay is observed in the pinned dependency source;
the research does not isolate all components of the approximately 304 ms result.
Chromium required execution outside the tracing sandbox, as in prior project
editor verification. The first direct sandboxed `code --version` attempt failed
at Chromium sandbox startup; the real probes subsequently passed with isolated
test profiles. Assertions were retained.

The existing host profiles were also run with:

```sh
PATH=/home/shmellyorc/.nvm/versions/node/v24.19.0/bin:$PATH \
VOIDLS_PATH="$PWD/build/ci-release/voidls" \
VSCODE_EXECUTABLE_PATH=/usr/share/code/code \
npm --prefix editors/vscode run test:host
```

## Reproduce Neovim and Vim compatibility probes

Downloads were official Neovim release `v0.12.6`'s
`nvim-linux-x86_64.tar.gz`, and a GitHub source archive of yegappan/lsp at
`c741b2f1afb42b9f267824dac4049674908f4a41`. They were extracted under
`/tmp/void-stage7-clients`; no user's package/runtime configuration was installed.
Version checks and recorded client SHA are in the results. Temporary downloads
are not permanent dependencies of the repository.

Create a disposable directory with these two files (LF, final newline):

```text
# main.void (omit this explanatory heading from the file)
use "/worker.void" as worker
func entry(x: i32) -> i32:
    return worker.calculate(x)

# worker.void (omit this explanatory heading from the file)
func calculate(x: i32) -> i32:
    return x + 1
```

Set `VOID_STAGE7_FIXTURE` to that directory, `VOIDLS_PATH` to the absolute server,
and `VOID_STAGE7_OUTPUT` to an absolute output JSON path. Then:

```sh
XDG_STATE_HOME=/tmp/void-stage7-nvim-state \
XDG_CACHE_HOME=/tmp/void-stage7-nvim-cache \
XDG_DATA_HOME=/tmp/void-stage7-nvim-data \
/tmp/void-stage7-clients/nvim-linux-x86_64/bin/nvim \
  --headless -u NONE -i NONE -l docs/research/stage7/nvim_probe.lua

# Also set these two environment variables for Vim:
# VOID_VIM_CLIENT=/tmp/void-stage7-clients/lsp-c741b2f1afb42b9f267824dac4049674908f4a41
# VOID_VIM_CLIENT_SHA=c741b2f1afb42b9f267824dac4049674908f4a41
vim -Nu NONE -i NONE -n -es -S docs/research/stage7/vim_probe.vim
```

Clean editor startup initially exposed probe setup problems: Neovim tried to
create its default read-only state directory, and disabled filetype detection
prevented client attachment; Vim needed a `.void` filetype rule for the worker
opened by navigation. Isolated XDG paths and explicit filetype rules corrected
those experiments. The Vim Ex probe flushes normal buffer listeners explicitly;
interactive typing/popup completion behavior is still an engineering test.

The Vim probe does not prove stale WorkspaceEdit rejection. Source inspection
found that the pinned client omits `workspaceEdit.documentChanges` and applies
TextDocumentEdit without a version check. This is a concrete roadmap constraint,
not a failure of the existing VOID server. Do not paper over it by advertising
safe rename to that client. UTF-16 handling also explicitly requires patch
9.0.1629 upstream; do not claim all Vim 9.0 builds are sufficient.

## Artifact dependency observation

Local Release `voidls` links static LLVM Support/Demangle plus system libraries.
`ldd` resolved `libm.so.6`, `libz3.so.4`, `libz.so.1`, `libzstd.so.1`,
`libstdc++.so.6`, `libgcc_s.so.1`, `libc.so.6` and the ELF loader. This is an
observation of the local distro LLVM build. It is not a dependency manifest for
official SDK Windows/macOS/Linux release bundles, which still need separate
clean-machine inspection and tests. No distribution optimization was applied.
