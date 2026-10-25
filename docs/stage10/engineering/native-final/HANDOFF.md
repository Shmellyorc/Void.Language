# Stage 10 final native acceptance handoff — 24 October 2026

## Subsequent native results and Windows console boundary

Windows stack CI #1 passed Debug/Release at
`b490579f84a702ec5d11da51a82934a498f88afa`. macOS stack CI #2 passed both ARM64
configurations at `383d2ce0a404a4bbf3df01d43097de100f5829a1` (run `40152738145`),
including exact inherited 1 MiB limits, Mach-O checks and original frontend/depth
tests. Those specific stack gaps are now evidenced; earlier unavailable statuses
below describe this handoff's original session and remain historical.

[Final Windows console checklist](../windows-console-final/CHECKLIST.md) preserves
the separate CI results, inventories existing probes, supplies direct-handle native
commands and an unexecuted read-only parent-state observer, and specifies the
remaining in-process observations. This Linux session has no interactive Windows
host; no native console/ConPTY result is invented. No virtualization investigation
was repeated. Stage 10 remains INCOMPLETE; stop for Golden Wrench/Boss review.

## Evidence corrections — 24 October 2026

Golden Wrench's [independent package review](../../../Stage10_Wrench_Review.md)
accepted the original package integrity and identified two provenance corrections.
The [correction record and separately named corrected package](../evidence-corrections/CORRECTIONS.md)
restore the authentic pre-CI #4 checkpoint and October 21 research baseline.
The original archive, patch and their hashes below remain immutable historical
review evidence. Their dated validation describes the pre-correction files;
use the corrected package and current hash receipts for the final documentation.

**STOPPED: no usable native Windows/macOS host is available in this session.
No native probe was executed. Stage 10 remains INCOMPLETE; release lock HELD.**
This addresses the [Golden Wrench Step 44 review](../../../GOLDEN_WRENCH_STAGE10_STEP44_REVIEW_2026-10-24.md):
the exact local package is now supplied, while native evidence gaps stay explicit.
No compiler, Runtime, test, workflow, original evidence or example was changed.

## Exact package for independent review

| File | Contents / verification |
|---|---|
| [local-closeout.patch](local-closeout.patch) | Original reviewed binary-capable diff against `92136acaf114a492452a7d84b31f174f5b305def`: all 15 modified and 24 added paths, including compressed CI logs. Read-only reverse-check passed against the pre-correction checkout. |
| [local-closeout.tar.gz](local-closeout.tar.gz) | All 39 complete changed files, with repository-relative paths; includes the example, EXAMPLES, acceptance, closeout results/validation, seven full compressed logs and CI metadata. All archive members were compared byte-for-byte with the source files. |
| [payload.sha256](payload.sha256) | Exact SHA-256 for every one of the 39 payloads, including the original inventory and rolling validation whose self-referential fields were null. |
| [package.json](package.json) | Baseline, patch/archive/manifest digests, membership and original evidence-record checks. This is Keyboard packaging verification, not Wrench reproduction. |
| [host-access.json](host-access.json) | Host/tool discovery, failed virtualization inventory attempts and exact native checks not executed. |
| [native-source-inputs.json](native-source-inputs.json) | Current receipt for 1,193 tracked non-documentation inputs, matching tested CI #4 source. The new example is separately frozen in the review payload. |
| [validation.json](validation.json) | Final local package, preservation, links and scope checks for this increment. |

Archive SHA-256:
`50c492f20740dd802f85d7e5a9a466f6c892f9fac91f7f121fd6d77ba41ae39f`.
Patch SHA-256:
`61607afa1d74cb7f1d10ab0cf59119399103938764fc3997f2ec4e844e8da921`.

For Wrench, unpack the archive into a separate inspection directory and run
`sha256sum -c /absolute/path/to/payload.sha256` from that directory. Inspect the
patch against the stated baseline; do not apply it to the live checkout. The
archive is an exact Step 43/44 snapshot, not a package of this later native-access
handoff. Its embedded inventory remains valid for that original archive; the live
39-file inventory now carries the corrected payload hashes. The newly
saved Step 44 review and earlier CI #4/design review are separate preserved inputs.
No independent reviewer approval is claimed for this newly supplied diff.

The original [28 command records](../closeout/results.json) were reconciled again:
no unexpected recorded failure; one expected `VOID4001` application-entry rejection.
Their binaries were existing local Debug/Release/strict builds, as recorded.
Those tests were not rerun for packaging. Wrench can now inspect their complete
argv/output, example source, probe source text and binary hashes in the archive.

## Actual native access result

The executable host is **Linux x86_64, 7.2.6-pikaos**. Codex project discovery exposes
only `local` execution entries for this checkout; ChatGPT projects supply no native
executor. No Windows/macOS SSH endpoint, guest credentials or native host was
provided. No speculative SSH connection was attempted.

`pwsh`, `powershell`, `xcrun`, `xcodebuild` and `sw_vers` are absent. Wine is present
but supplies no native acceptance and was not used. `VBoxManage` is installed;
both read-only `list vms` and `list runningvms` timed out after 15 seconds. Registered
and running VM inventories therefore remain **unknown**, not proven empty. No VM
was started, modified or reconfigured. No usable native guest session was obtained.

The scoped `build/*/voidc.exe` and `build/*/void_tests.exe` searches returned no
native compiler/frontend binaries. Local LLVM 19 readback tools cannot establish
the pertinent Windows PE reserve without those binaries and provenance. The
preserved CI #4 logs contain no Windows StackReserve readback or macOS RLIMIT
receipt; their Linux stack commands remain separate evidence.

## Bounded execution proposal — NOT EXECUTED

Use the [existing Step 42 commands](../step42-native-commands.md) and unchanged
tests. Their full unfiltered acceptance commands and pre-CI96 source receipt are
historical: this proposal selects only the missing boundaries and uses the current
receipt above. It requests no seven-job workflow or workflow edit. Boss selects
the native hosts or any hosted mechanism before execution.

**Source and build gate, both platforms.** Use tested source
`90c0cf64b9aba60d7c642096d2aaac45aae70d56` (or the current checkout with identical
tracked non-doc inputs). Copy the receipt with the source; verify every entry
before and after the focused probes, preserving exact bytes and LF fixtures:

```sh
python -c 'import hashlib,json,pathlib,sys; p=pathlib.Path; m=json.loads(p(sys.argv[1]).read_text())["sha256"]; bad=[f for f,h in m.items() if not p(f).is_file() or hashlib.sha256(p(f).read_bytes()).hexdigest()!=h]; print(json.dumps({"inputs":len(m),"mismatches":bad})); sys.exit(bool(bad))' docs/stage10/engineering/native-final/native-source-inputs.json
```

Run from the native repository root; `python` means that host's installed Python.
Record OS/architecture, SDK/LLVM identity, CMake configuration, commands and exit
codes. Reuse only identifiable native builds with matching inputs and CMake
configuration. If absent, configure separate Debug/Release directories using the
existing Step 42 LLVM **19.1.7**, Ninja, platform SDK and sanitizer settings. Build
the necessary existing targets with `cmake --build ... --parallel 3`. Do not
change optimization, linkers, stack reserves, guards, timeouts or test sources.

**Windows x64 Debug and Release: two targeted reserve/probe records.** Build/reuse
`voidc`, `void_tests` and, for the interactive portion, `void_runtime_console_keyboard`.
In the native developer PowerShell, set `$b` to the exact configuration directory
and use the existing pinned SDK readback command:

```powershell
& .toolchains/llvm-19.1.7/bin/llvm-readobj.exe --file-headers "$b/voidc.exe" "$b/void_tests.exe"
if ($LASTEXITCODE -ne 0) { throw 'PE readback failed' }
ctest --test-dir $b -R '^frontend$' --output-on-failure
if ($LASTEXITCODE -ne 0) { throw 'Frontend failed' }
python tests/integration/semantic_depth.py "$b/voidc.exe"
if ($LASTEXITCODE -ne 0) { throw 'Semantic depth failed' }
Get-FileHash -Algorithm SHA256 "$b/voidc.exe", "$b/void_tests.exe"
```

Retain the full PE headers, per-command status and native file hashes. Inspect
**both actual StackReserve values: exactly 1,048,576 (0x100000)**, not StackCommit
or a configured linker default. Any mismatch fails this gate; do not relink with
another reserve to manufacture a pass. Run unchanged frontend and semantic-depth
probes for each newly built configuration; CI #4 remains the full regression gate.
If Boss supplies the exact CI #4 PE binaries with a trustworthy identity receipt,
their reserve can also be read offline with LLVM 19 without Windows execution;
that establishes PE metadata only, not console behavior or a new native test pass.

**macOS ARM64 Debug and Release: two explicit 1 MiB probe records.** Build/reuse
`voidc` and `void_tests`; set `b` and `out` as in Step 42. Keep its existing
`checked` helper, which stores stdout/stderr and command exit status. Execute its
original bounded-stack block unchanged in each configuration:

```bash
(
    ulimit -S -s 1024
    ulimit -H -s 1024
    checked stack-budget python -c 'import resource; assert resource.getrlimit(resource.RLIMIT_STACK)==(1048576,1048576); print(resource.getrlimit(resource.RLIMIT_STACK))'
    checked frontend-stack ctest --test-dir "$b" -R '^frontend$' --output-on-failure
    checked semantic-depth python tests/integration/semantic_depth.py "$b/voidc"
)
```

Use the original `set -euo pipefail` shell context. Retain both observed limits,
original frontend output, semantic-depth accepted/rejected/recovery output, exits
and SHA-256 of `voidc`/`void_tests`. These limits are inherited by CTest/Python and
their unchanged children. The existing semantic-depth helper explicitly sets its
own limits only on Linux; the native macOS parent must establish both values.
Inability to establish the exact limit is a failed/unavailable gate, never a pass.

**Real Windows console/ConPTY: existing-probe scope, Debug and Release.** Use an
actual native interactive host with input/output attached to the intended console.
Record Windows/terminal host versions and genuine ConPTY arrangement separately.
Do not pipe or redirect the tested process's standard handles when claiming wide
console output: that selects the redirected path. Use a host capture facility
that preserves its actual standard-handle kinds, or keep the result unverified.
Existing Step 42 native commands are:

```powershell
& "$b/void_runtime_console_keyboard.exe" 1 closed
& "$b/void_runtime_console_keyboard.exe" 1 unclosed
& "$b/void_runtime_console_keyboard.exe" shared
& "$b/voidc.exe" run tests/stage10/console_terminal.void
& "$b/voidc.exe" run tests/stage10/console_keys.void
```

Supply input only after READY within the **existing** 2 s / 5 s deadlines; the
shared probe expects `é`, newline, `Z`, newline. The source key fixture consumes
ten events. Use a disposable terminal for presentation tests and retain their
cleanup/error output. Do not insert sleeps or extend deadlines. Failures must be
recorded before any proposal to change source.

The native key probe exercises real nested mode ownership, non-LIFO rejection,
explicit restoration and unclosed-session shutdown. The shared probe additionally
exercises the real process queue, mixed key/line input, concurrent Runtime output,
GC roots and shutdown assertions. Presentation/source fixtures exercise real
capability and error paths. These may supply focused host evidence once executed.
They do **not** by themselves record before/after GetConsoleMode/code pages or
every native error/progress value. The key probe prints known-flags and repeat
counts, but not the actual modifier bitmask or key-down boolean. A zero exit is
therefore insufficient to close the full modifier/state-restoration gate.

Keep actual ReadConsoleW/WriteConsoleW observation distinct from the key path's
ReadConsoleInputW. The existing Windows `runtime_console_transfers` and
`runtime_console_input` normal invocations use injected adapters; their standalone
native transfer/input modes are POSIX-only. No existing automated ConPTY driver
or native before/after code-page observer was found. Real Windows partial/error
counts, synchronous/overlapped borrowed-handle cancellation, retained entered-I/O
buffers and cleanup require actual native receipts with a suitable host observer.
Where the existing probes cannot expose a required fact, stop and return that
specific gap for Boss/Wrench rather than add a framework or claim an injected pass.

## Outstanding disposition

All three gates remain **UNVERIFIED** here: Windows PE reserve; macOS Debug/Release
1 MiB probes; real Windows console/ConPTY/state/handle behavior. Native Vim/Neovim
and a real second Windows volume retain their previous exclusions. Nothing in
this handoff changes CI #4's 7/7 and 1,383/1,383, the separate CI #98/experiment #3
failures, original Step 41 protections, Runtime behavior or future-design scope.

Boss can supply native host access (including an actual interactive Windows host)
or exact native binaries for the narrower PE readback. Golden Wrench reviews this
package and the bounded native proposal; Boss decides execution and release.
No commit, push, branch, dispatch, main-workflow change or next stage occurred.
