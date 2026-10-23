# Step 42 native acceptance preparation — 23 October 2026

**PREPARED, NOT EXECUTED. Windows and macOS: UNVERIFIED. Stage 10: INCOMPLETE.**
Boss reports Golden Wrench accepted Step 41. Its final Linux results remain
Debug 199/199, Release 199/199 and strict ASan/UBSan/leaks 199/199. This record
does not extend that acceptance to another operating system.

## Execution gate and source identity

No native Windows/macOS host is available in this session. No hosted job was
dispatched. Boss must authorize native execution and supply the exact source
snapshot/ref before these commands run. The current worktree includes cumulative
uncommitted Stage 10 work: checking out the existing HEAD alone is insufficient.
Transfer the approved source snapshot with exact bytes; preserve LF in source
files, including on Windows. Do not normalize fixtures or rewrite accepted logs.
Use a fresh build directory for each native configuration.

The existing `Compiler foundation` workflow has **seven** unconditional matrix
entries, not a four-native-job dispatch option. Running it as currently written
also runs three Linux jobs. Boss must explicitly choose that full workflow,
authorize a separately reviewed matrix-selection change, or provide native hosts
on which to run the existing commands below. This preparation changes no workflow.
The manual Windows path and phase23 stack-probe workflows are diagnostics, not
substitutes for Stage 10 acceptance. No commit, push, branch or dispatch is implied.

Before building and again after acceptance, run this command from the repository
root in either shell below. Set `out` to the configuration's evidence directory.
Keep the first receipt as `source-before.json`, the second as `source-after.json`.
It verifies all 766 accepted build/test/editor inputs and records actual host
identity. It deliberately uses the code freeze, since this increment updates docs.

```text
python -c 'import hashlib,json,pathlib,platform,sys; p=pathlib.Path; h=lambda f:hashlib.sha256(p(f).read_bytes()).hexdigest(); m=json.loads(p("docs/stage10/engineering/step41-full-source-freeze.json").read_text()); bad=[f for f,s in m.items() if not p(f).is_file() or h(f)!=s]; r={"host":platform.platform(),"system":platform.system(),"machine":platform.machine(),"inputs":len(m),"mismatches":bad,"freeze_sha256":h("docs/stage10/engineering/step41-full-source-freeze.json"),"compiler_cpp_sha256":h("src/frontend/compiler.cpp"),"step41_results_sha256":h("docs/stage10/engineering/step41-results.json")}; p(sys.argv[1]).write_text(json.dumps(r,indent=2)+"\n"); assert not bad and r["freeze_sha256"]=="62437fdbff0a9f5a27d849ae35e439f7a1fb2da80a64d84d7b9ef6272c464708" and r["step41_results_sha256"]=="e7eb5375aea4eb705ac275aa6043fb9a0bd3c0336cd71ef39869c8750db9b688",r' "$out/source-before.json"
```

Expected Step 41 results SHA-256:
`e7eb5375aea4eb705ac275aa6043fb9a0bd3c0336cd71ef39869c8750db9b688`.
Expected repaired `compiler.cpp`:
`9a54df7f1778446fc3d593829978b4180df2ffd4a55f92ebaae156fc37f81d32`.
An identity mismatch stops acceptance; retain the receipt and resolve it first.

## Windows x64 Debug and Release

Use real Windows (existing CI: windows-2022), Python 3.12, CMake/Ninja and the
existing x64 MSVC/Windows SDK developer environment. CI already enters it using
`ilammy/msvc-dev-cmd@v1` with `arch: x64`. Preserve `/MD`, iterator level 0,
LLVM RTTI compatibility, both longPathAware manifests and the actual DIA library.
Do not substitute a compiler-only LLVM installer or remove SDK link dependencies.

The following is for a PowerShell 7 native developer shell, repository root.
It reuses `tools/ci/llvm_sdk.py`; the local environment files emulate the existing
workflow's environment handoff without evaluating their contents as shell code.
Run each configuration sequentially, setting `cfg` to `Debug`, then `Release`.
The logging helper only executes existing commands and fails on any nonzero exit.

```powershell
$ErrorActionPreference = 'Stop'
python -c 'import platform; assert platform.system()=="Windows" and platform.machine().lower() in ("amd64","x86_64")'
if ($LASTEXITCODE -ne 0) { throw 'Requires native Windows x64' }
$cfg = 'Debug' # Repeat the whole configuration with Release, without reusing Debug's build.
$b = "build/step42-windows-$cfg"
$out = "build/step42-evidence/windows-$cfg"
New-Item -ItemType Directory -Force $out | Out-Null
function Checked([string]$label, [string]$command, [string[]]$argv) {
    & $command @argv 2>&1 | Tee-Object -FilePath "$out/$label.log"
    $rc = $LASTEXITCODE
    $rc | Set-Content "$out/$label.exit.txt"
    if ($rc -ne 0) { throw "$label failed ($rc); retain the log" }
}
# Run the shared source receipt command above here, before SDK/build work.
$env:GITHUB_ENV = [IO.Path]::GetFullPath("$out/sdk.env")
$env:GITHUB_PATH = [IO.Path]::GetFullPath("$out/sdk.path")
Set-Content -Path $env:GITHUB_ENV -Value ''
Set-Content -Path $env:GITHUB_PATH -Value ''
Checked 'sdk' 'python' @('tools/ci/llvm_sdk.py')
Get-Content $env:GITHUB_ENV | Where-Object { $_ -match '=' } | ForEach-Object {
    $pair = $_ -split '=', 2
    [Environment]::SetEnvironmentVariable($pair[0], $pair[1], 'Process')
}
$env:PATH = ([IO.Path]::GetFullPath('.toolchains/llvm-19.1.7/bin')) + ';' + $env:PATH
$env:ASAN_OPTIONS = 'detect_leaks=1:halt_on_error=1:exitcode=87'
$env:UBSAN_OPTIONS = 'halt_on_error=1:print_stacktrace=1:exitcode=86'
Checked 'clang-version' $env:VOID_CLANG @('--version')
Checked 'cmake-version' 'cmake' @('--version')
Checked 'ninja-version' 'ninja' @('--version')
Checked 'configure' 'cmake' @('-S','.', '-B',$b, '-G','Ninja', "-DCMAKE_BUILD_TYPE=$cfg", '-DBUILD_TESTING=ON', '-DVOID_SANITIZERS=OFF', '-DVOID_LLVM_RESEARCH=OFF', '-DVOID_RETENTION_BASELINE=OFF', "-DLLVM_DIR=$env:LLVM_DIR", "-DVOID_CLANG=$env:VOID_CLANG", '-DVOID_LINKER=')
Checked 'build' 'cmake' @('--build',$b,'--parallel','3')
ctest --test-dir $b --show-only=json-v1 | Set-Content -Encoding utf8 "$out/inventory.json"
if ($LASTEXITCODE -ne 0) { throw 'Inventory failed' }
Checked 'ctest' 'ctest' @('--test-dir',$b,'--output-on-failure','--output-junit',([IO.Path]::GetFullPath("$out/junit.xml")))
Checked 'pe-headers' '.toolchains/llvm-19.1.7/bin/llvm-readobj.exe' @('--file-headers',"$b/voidc.exe","$b/void_tests.exe")
# Inspect pe-headers.log: both StackReserve values must be 1048576 (0x100000).
# If different, stop this gate; do not increase a reserve or claim the 1 MiB pass.
Checked 'frontend-stack' 'ctest' @('--test-dir',$b,'-R','^frontend$','--output-on-failure')
Checked 'semantic-depth' 'python' @('tests/integration/semantic_depth.py',"$b/voidc.exe")
Checked 'workload-build' 'cmake' @('--build',$b,'--target','void_format_workloads','--parallel','3')
Checked 'workloads' "$b/void_format_workloads.exe" @()
Checked 'path-probe-build' 'cmake' @('--build',$b,'--target','void_windows_path_probe','void_windows_path_probe_aware','--parallel','3')
Checked 'path-probe' 'python' @('tests/stage9/windows_path_probe.py',"$b/void_windows_path_probe.exe","$b/void_windows_path_probe_aware.exe",'--output',"$out/windows-paths.json")
# Run the shared source receipt again, with source-after.json as its output.
```

Windows native sanitizer execution is not claimed: existing Windows CI uses
`VOID_SANITIZERS=OFF`. Setting sanitizer environment variables preserves workflow
settings; it does not instrument these binaries. Preserve the accepted strict Linux
results separately. Record Windows build number, SDK/toolset versions, Ninja and
CMake versions, the complete LLVM SDK identity, and native console host/version.

The path probe is observational: inspect its operation results, not just its exit
status. Record LongPathsEnabled without changing registry/security policy. Preserve
the restricted-token, private TMP/TEMP DACL, Unicode argv, 8.3/reparse/hardlink and
long-path checks in `application_workflow_stage9`, `native_output_paths_ci88`,
`long_physical_paths_ci89`, `lsp_stage8`, `local_libraries` and LSP URI suites.
The genuine second-volume publication test needs an available additional volume;
without one it remains UNVERIFIED. Stage 9 CI95's 320 passes are historical only.

## macOS ARM64 Debug and Release

Use a real native ARM64 host (no Rosetta/x86_64 substitution), Xcode command-line
SDK, Python 3.12, CMake and Ninja. Existing CI checks the actual architecture even
though the runner label is `macos-latest`. These commands use bash from repository
root. Run once for Debug, then once for Release in separate fresh build directories.

```bash
set -euo pipefail
python -c 'import platform; assert platform.system()=="Darwin" and platform.machine().lower() in ("arm64","aarch64")'
cfg=Debug # Repeat the whole configuration with Release.
b="build/step42-macos-$cfg"
out="build/step42-evidence/macos-$cfg"
mkdir -p "$out"
checked() {
    local label="$1"; shift
    local rc=0
    "$@" >"$out/$label.log" 2>&1 || rc=$?
    printf '%s\n' "$rc" >"$out/$label.exit.txt"
    if [ "$rc" -ne 0 ]; then cat "$out/$label.log"; return "$rc"; fi
}
# Run the shared source receipt command above here, before SDK/build work.
export GITHUB_ENV="$PWD/$out/sdk.env" GITHUB_PATH="$PWD/$out/sdk.path"
: >"$GITHUB_ENV"
: >"$GITHUB_PATH"
checked sdk python tools/ci/llvm_sdk.py
while IFS='=' read -r key value; do
    [ -z "$key" ] || export "$key=$value"
done <"$GITHUB_ENV"
export PATH="$PWD/.toolchains/llvm-19.1.7/bin:$PATH"
export ASAN_OPTIONS=detect_leaks=1:halt_on_error=1:exitcode=87
export UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1:exitcode=86
checked clang-version "$VOID_CLANG" --version
checked cmake-version cmake --version
checked ninja-version ninja --version
checked os-version sw_vers
checked sdk-version xcrun --show-sdk-version
checked configure cmake -S . -B "$b" -G Ninja "-DCMAKE_BUILD_TYPE=$cfg" -DBUILD_TESTING=ON -DVOID_SANITIZERS=OFF -DVOID_LLVM_RESEARCH=OFF -DVOID_RETENTION_BASELINE=OFF "-DLLVM_DIR=$LLVM_DIR" "-DVOID_CLANG=$VOID_CLANG" -DVOID_LINKER=
checked build cmake --build "$b" --parallel 3
ctest --test-dir "$b" --show-only=json-v1 >"$out/inventory.json"
checked ctest ctest --test-dir "$b" --output-on-failure --output-junit "$PWD/$out/junit.xml"
(
    ulimit -S -s 1024
    ulimit -H -s 1024
    checked stack-budget python -c 'import resource; assert resource.getrlimit(resource.RLIMIT_STACK)==(1048576,1048576); print(resource.getrlimit(resource.RLIMIT_STACK))'
    checked frontend-stack ctest --test-dir "$b" -R '^frontend$' --output-on-failure
    checked semantic-depth python tests/integration/semantic_depth.py "$b/voidc"
)
checked workload-build cmake --build "$b" --target void_format_workloads --parallel 3
checked workloads "$b/void_format_workloads"
checked macho file "$b/voidc" "$b/voidls"
checked native-dependencies otool -L "$b/voidc" "$b/voidls"
# Run the shared source receipt again, with source-after.json as its output.
```

The existing semantic-depth Python helper sets RLIMIT itself only on Linux.
The macOS child shell above establishes the genuine 1 MiB budget for both original
probes without modifying tests or guards. Failure to establish it is a failed gate.
Do not confuse phase23 machine-code stack research with frontend stack acceptance.
macOS Debug/Release use the existing non-sanitized platform configurations; no new
native sanitizer pass is claimed. Preserve the Linux strict acceptance separately.

Keep the five real POSIX console tests, source PTY cases, broken-pipe/SIGPIPE and
termios restoration checks. Darwin's production SIGPIPE path uses pending-signal
inspection plus `sigwait`, not unavailable `sigtimedwait`. Terminal descriptions
are dynamically loaded from the system ncurses facility; missing support must be
reported conservatively. Capture TERM and capability snapshots. Source timezone
semantics still use embedded 2026e rules, not the host database. `lsp_stage8` tests
canonical aliases modelling `/var -> /private/var`; execute it on macOS itself.
`native_output_paths_ci88` retains POSIX ancestor-symlink output checks.

## What the unfiltered suites establish, and what they do not

Use `step42-inventory.json` for the complete expected name list and contract map.
Projected default inventory: Windows 194, macOS 199. These are source-derived
expectations, **not native discovery results**. Save actual CTest JSON and JUnit;
investigate differences. No name filter, exclusion, larger timeout, disabled safety
check, changed optimization flag or test replacement is authorized by this record.

- Fields/operators: private/const/init/readonly, generics, all glyphs and compounds,
  recursive dictionary-key exclusion, independent malformed IR, worker/effects,
  checked failures, selected places, precise roots, cache/reload/generation leases.
- Display: primitive/custom/composite/enum/result, interpolation and source spans,
  allocations/cycles/GC, numeric and temporal patterns, embedded zone rules.
- Console: injected native faults/wide conversion, shared queue, EOF/CR/Unicode,
  cancellation/lifetime/shutdown, source print/cleanup and exact output bytes.
  `stage10_console_aot`, `stage10_operators_aot`, `stage10_conversion_aot`,
  `stage10_temporal_aot` include normal/O2 JIT and independently linked execution;
  source application build tests cover production executable startup/packaging.
- `stage10_temporal_install` installs compiler/tooling/stdlib/licenses into a
  Unicode prefix, relocates it, exercises installed LSP and stdlib discovery, and
  runs a moved executable without external timezone data. Retain all of it.
- Existing async/GC/native/filesystem/LSP/formatting suites remain in the full run.
  Linux-only `--wrap` GC fault hooks are not present on these hosts; ordinary
  portable GC/fault regressions remain. Do not describe those as identical hooks.

**Windows console hardware/host gate remains additional.** The five POSIX-only
tests are not registered on Windows. `runtime_console_wide` calls the production
transcoder through injected adapters; it is not evidence of actual ReadConsoleW/
WriteConsoleW. There is no automated CreatePseudoConsole/ConPTY driver in the
current tests. A green Windows CTest run alone cannot close this gate.

On traditional console, Windows Terminal and a genuine ConPTY host, record host
versions, actual endpoint types/capabilities, GetConsoleMode and input/output code
pages before and after (including failure/shutdown). Use the existing executables:

```powershell
& "$b/void_runtime_console_keyboard.exe" 1 closed
& "$b/void_runtime_console_keyboard.exe" 1 unclosed
& "$b/void_runtime_console_keyboard.exe" shared
& "$b/voidc.exe" run tests/stage10/console_terminal.void
& "$b/voidc.exe" run tests/stage10/console_keys.void
```

For the key probes supply input after READY and within the existing deadlines;
the shared probe expects `é` followed by newline and `Z` plus newline. Preserve
records, errors and exact prior modes; a manual visual observation is not byte or
native state verification. The source terminal fixture deliberately changes title,
colors and cursor visibility; run in a disposable test console and restore the
test host's presentation afterwards. Also retain these not-yet-automated gates:

- Actual wide-console cooked/raw input, supplementary scalars, NUL/BOM, pending
  cooked input, physical key up/repeat/modifiers, mixed raw/text carry transitions,
  every echo/line/control-C combination and canonical editor preservation.
- Real ReadConsoleW/WriteConsoleW partial/error counts, native UTF-16 boundaries,
  failed restoration, original LastError and no code-page/host policy changes.
- Synchronous and overlapped borrowed file/pipe handles, EOF/short/error progress,
  queued cancellation, unrelated host I/O untouched, retained buffers for entered
  uncancellable syscalls and NativeSafe GC during blocked reads. Shutdown may
  legitimately wait for external progress under the approved contract.
- Independent stdout/stderr capabilities, unsupported redirected presentation
  emitting no controls, accessible cursor response ownership and bounded waits.

These require native host interaction/instrumentation; the existing probes cover
parts, not every row. Record UNVERIFIED for any row without actual evidence.
No new console test framework or simulated pass is supplied here.

## Editor and evidence closeout (each native platform)

For Debug, reuse existing Node 24.19.0 extension steps, repository root:

```text
cd editors/vscode
npm ci
npm test
npm run test:host
npm run package
```

Set `VOIDLS_TEST_SERVER` and `VOIDLS_PATH` to the absolute newly built native
`voidls[.exe]` before these commands. Record each exit status/log and actual VS Code
host versions/profiles (trusted/restricted/multi-root). Use installed real Neovim/
Vim clients if available through existing adapter tests; absence is UNVERIFIED.
Do not claim Linux's editor results as native passes or erase deferred F12/F2/
hover/documentation observations.

Existing headless adapter commands, with actual native client paths substituted:

```text
python tests/editors/run.py --server NATIVE_VOIDLS --nvim NATIVE_NVIM --vim NATIVE_VIM --vim-client PINNED_LSP_CLIENT --output AUTOMATIC_JSON
python tests/editors/run.py --server NATIVE_VOIDLS --nvim NATIVE_NVIM --vim NATIVE_VIM --vim-client PINNED_LSP_CLIENT --output EXPLICIT_JSON --explicit-entry
```

Use the same pinned Vim LSP client as Step41 (revision
`c741b2f1afb42b9f267824dac4049674908f4a41`); do not download or update it in this
preparation. Omit an unavailable client argument and record that client UNVERIFIED.
The existing `tests/editors/neovim_automatic.py` and `vim_automatic.py` use POSIX
PTYs; run them on macOS with native clients and output paths as in Step41.
They cannot certify Windows terminal menus. Windows terminal-editor acceptance
requires actual client/console evidence; an unavailable harness remains a gate.

Windows: `$env:VOIDLS_TEST_SERVER = [IO.Path]::GetFullPath("$b/voidls.exe")` and
`$env:VOIDLS_PATH = $env:VOIDLS_TEST_SERVER`. macOS:
`export VOIDLS_TEST_SERVER="$PWD/$b/voidls" VOIDLS_PATH="$PWD/$b/voidls"`.
Set these before changing into `editors/vscode`, then return to the repository
root before closeout. Preserve each npm command's nonzero status as a failure.

From repository root, record final built binary/archive and SDK identities with
the same command on either host (in addition to any retained AOT outputs):

```text
python -c 'import hashlib,json,pathlib,sys; p=pathlib.Path; b=p(sys.argv[1]); sdk=p(".toolchains/llvm-19.1.7"); fs=[f for f in b.iterdir() if f.is_file() and f.name.startswith(("void","libvoid")) and f.suffix in ("",".exe",".lib",".a")]; fs += [f for f in (sdk/"bin").glob("clang*") if f.is_file() and f.suffix in ("",".exe")]; fs += [f for f in (sdk/"lib").glob("*LLVMCore*") if f.is_file()]; assert fs; r={str(f):hashlib.sha256(f.read_bytes()).hexdigest() for f in sorted(set(fs))}; p(sys.argv[2]).write_text(json.dumps(r,indent=2)+"\n")' "$b" "$out/binaries.json"
```

Archive the evidence directory plus CMakeCache.txt, CMake configure logs,
build.ninja, CTestTestfile.cmake, Testing/Temporary/LastTest.log and failed-test
logs, raw workload/retention/cache records produced under the build directory,
SDK environment/version logs, full build stdout/stderr and real console transcripts.
Record SHA-256 for the final compiler, voidls, test executables, core/Runtime/native
fixture archives and SDK compiler/LLVMCore, plus any generated AOT executables
retained by the existing tests. Windows/macOS binaries will have their own hashes;
they must not equal Linux hashes to qualify. Re-run the source receipt at closeout.
Use actual JUnit case outcomes and skip reasons; partial/aborted runs never count
as completed acceptance. Preserve original failing artifacts before any minimal
repair; a source repair requires matching rebuilds and appropriate reacceptance.

No native binary identity or native test result exists for this increment yet.
Preserve Step 37 anomalies, transient pinned Clang crash/reproducer, deep collection
compile cost and Release LLD GVN trace (original build exit 0, no retry, cause
unknown). Deferred usability/optimization and editor objectives remain deferred.
Return these native artifacts for Wrench review before Steps 43–44.
