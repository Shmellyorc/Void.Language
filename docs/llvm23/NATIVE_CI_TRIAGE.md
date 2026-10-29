# Native CI failure burn-down

**Baseline:** `5198c7ac9c256fd05f53ff5ba594c38dc0093b7f`. [Original run 40861888611](https://github.com/Shmellyorc/Void.Language/actions/runs/40861888611). No hosted rerun is part of this sprint.

## Evidence and complete inventory

All four complete job logs and CTest artifacts were retrieved. Debug/Release names match exactly: Windows 58/319 each; macOS ARM64 12/362 each. Ubuntu Debug, Release and sanitizers passed in the original run. Raw archived bytes are retained locally in `.toolchains/burndown-40861888611`; the verbatim failure blocks below preserve invalid output bytes using backslash escapes. Native success remains pending a Boss-controlled run.

| Platform/config | Job | Artifact | SHA-256 (zip) |
| --- | --- | --- | --- |
| Windows-X64-Debug | 123406954943 | 12896235835 | `16099d368ce6a377fb8eebaf3092c21b077b888a4f177b4b74c3390afae7fd6f` |
| Windows-X64-Release | 123406954689 | 12894416518 | `e30ad2c484c7fc3ebec98128fadd61ca4e3d57208585fec6346e69c431162345` |
| macOS-ARM64-Debug | 123406955222 | 12896663336 | `2a09b7815f0733add06da58839117cfbd020a0ec42f6086aae252eaa27dd99ed` |
| macOS-ARM64-Release | 123406955201 | 12893359144 | `104e12f03d240ce6688dabef90f0c555951897fcdbe1694cde6b3cb20b4cbb78` |

| Family | Classification | Initial diagnosis | Required verification |
| --- | --- | --- | --- |
| F1 | platform integration | Windows manifest passes unsupported -pthread | Generate platform flags; rerun all warning-strict AOT siblings. |
| F2 | harness contract | Native CRT text CRLF versus LF expectation | Identify each text-only suffix/status; retain exact runtime, wire and log bytes. |
| F3 | harness defect | Implicit locale encoding for Unicode fixtures | Explicit UTF-8 at source/manifest reads and writes; run with cp1252 locale. |
| F4 | harness identity | Canonical receipt/diagnostic paths versus lexical aliases | Compare canonical filesystem identities; retain command argv and byte/hash assertions. |
| F5 | harness launch | Relative Python executable interpreted under a different launch directory | Use absolute interpreter; preserve executable hash and exact command provenance. |
| F6 | harness oracle | Windows historical fromtimestamp range | Exact UTC epoch arithmetic, retaining historical/boundary cases. |
| F7 | platform harness | Darwin rejects shrinking an already running Python launcher stack (the Python EINVAL message does not prove the inherited maximum) | Query inherited and effective limits; enforce supported hard stack and report it. |
| F8 | possible production defect | Windows cache unavailable/unpublished | Inspect secure native cache operations and exact miss/hit/bypass behavior. |
| F9 | possible production defect | Closed Windows pipe loops until timeout | Inspect native write error mapping and ownership; verify closed peer and file independence. |
| F10 | unresolved filesystem contract | Invalid native filename enumeration succeeds on macOS | Check creation syscall and actual returned names before production changes. |
| F11 | harness registration | JSON preservation runs random preservation script | Restore intended JSON preservation coverage and execute synthetic incidents. |

### Windows-X64 (both Debug and Release)

| Failed test | Family | Original exception/assertion (full output linked) |
| --- | --- | --- |
| `math_execution` | F1 | AssertionError: b"clang: warning: argument unused during compilation: '-pthread' [-Wunused-command-line-argument]\r\n" |
| `math_frontend` | F4 | AssertionError: b"C:/Users/runneradmin/AppData/Local/Temp/VOID math frontend fyi2_39m/case2.void:4:20: error: Math function 'is_nan' requires exactly f32 or f64.\r\n    code: VOID1103\r\n\r\n 4 \|     var bad = math.is_nan(input)\r\n   \|           ... |
| `math_errors` | F4 | AssertionError: b'D:/a/Void.Language/Void.Language/stdlib/math.void:49:14: runtime error: Signed minimum abs is out of range\r\nIn function $numeric::i8::abs\r\nCalled from /@stdlib/math.void::abs\r\nD:/a/Void.Language/Void.Language/stdlib/math.vo... |
| `math_worker` | F2 | AssertionError: (['D:\\a\\Void.Language\\Void.Language\\build\\Debug\\void_io_runner.exe', '--async', 'C:\\Users\\RUNNER~1\\AppData\\Local\\Temp\\VOID math worker l7a7jr2p\\worker.void'], b'Debug source execution passed\r\nO2 source execution pass... |
| `math_install` | F2 | AssertionError: (['C:\\Users\\RUNNER~1\\AppData\\Local\\Temp\\VOID math relocated fwlqespg\\relocated install\\bin\\voidc.exe', 'run', 'C:\\Users\\RUNNER~1\\AppData\\Local\\Temp\\VOID math relocated fwlqespg\\installed_math.void', '2', '--no-opt']... |
| `math_preservation` | F4 | AssertionError |
| `os_errors` | F3 | UnicodeEncodeError: 'charmap' codec can't encode character '\u96ea' in position 1337: character maps to <undefined> |
| `lsp_os` | F3 | UnicodeEncodeError: 'charmap' codec can't encode character '\U0001f600' in position 48: character maps to <undefined> |
| `os_owner` | F2 | AssertionError: (['D:\\a\\Void.Language\\Void.Language\\build\\Debug\\void_io_runner.exe', '--async', 'C:\\Users\\RUNNER~1\\AppData\\Local\\Temp\\VOID OS owner 37vbs9_t\\owner.void'], b'Debug source execution passed\r\nO2 source execution passed\r... |
| `os_examples` | F1 | AssertionError: b"clang: warning: argument unused during compilation: '-pthread' [-Wunused-command-line-argument]\r\n" |
| `os_install` | F1 | AssertionError: b"clang: warning: argument unused during compilation: '-pthread' [-Wunused-command-line-argument]\r\n" |
| `random_execution` | F1 | AssertionError: b"clang: warning: argument unused during compilation: '-pthread' [-Wunused-command-line-argument]\r\n" |
| `random_install` | F1 | AssertionError: b"clang: warning: argument unused during compilation: '-pthread' [-Wunused-command-line-argument]\r\n" |
| `random_errors` | F4 | AssertionError: b'D:/a/Void.Language/Void.Language/stdlib/convert.void:55:12: runtime error: convert.unwrap received Err; inspect the result or use unwrap_or\r\n    return _convert_unwrap<t, e>(outcome)\r\n           ^\r\nIn function /@stdlib/conv... |
| `random_preservation` | F4 | StopIteration |
| `random_worker` | F2 | AssertionError: (['D:\\a\\Void.Language\\Void.Language\\build\\Debug\\void_io_runner.exe', '--async', 'C:\\Users\\RUNNER~1\\AppData\\Local\\Temp\\VOID random worker xm548pen\\worker.void'], b'Debug source execution passed\r\nO2 source execution pa... |
| `binary_execution` | F1 | AssertionError: ([WindowsPath('D:/a/Void.Language/Void.Language/build/Debug/voidc.exe'), 'build', WindowsPath('C:/Users/RUNNER~1/AppData/Local/Temp/VOID binary \xe9\U0001f600 fkgbiedw/main.void'), '-o', WindowsPath('C:/Users/RUNNER~1/AppData/Local... |
| `combined_execution` | F1 | AssertionError: b"clang: warning: argument unused during compilation: '-pthread' [-Wunused-command-line-argument]\r\n" |
| `memory_execution` | F1 | AssertionError: ([WindowsPath('D:/a/Void.Language/Void.Language/build/Debug/voidc.exe'), 'build', WindowsPath('D:/a/Void.Language/Void.Language/tests/memory/semantics.void'), '-o', WindowsPath('C:/Users/RUNNER~1/AppData/Local/Temp/VOID memory \xe9... |
| `text_encoding_text` | F1 | AssertionError: b"clang: warning: argument unused during compilation: '-pthread' [-Wunused-command-line-argument]\r\n" |
| `text_encoding_hex` | F1 | AssertionError: b"clang: warning: argument unused during compilation: '-pthread' [-Wunused-command-line-argument]\r\n" |
| `text_encoding_base64` | F1 | AssertionError: b"clang: warning: argument unused during compilation: '-pthread' [-Wunused-command-line-argument]\r\n" |
| `text_encoding_ascii` | F1 | AssertionError: b"clang: warning: argument unused during compilation: '-pthread' [-Wunused-command-line-argument]\r\n" |
| `text_encoding_oracles` | F1 | AssertionError: b"clang: warning: argument unused during compilation: '-pthread' [-Wunused-command-line-argument]\r\n" |
| `text_encoding_interop` | F1 | AssertionError: b"clang: warning: argument unused during compilation: '-pthread' [-Wunused-command-line-argument]\r\n" |
| `text_encoding_large` | F1 | AssertionError: b"clang: warning: argument unused during compilation: '-pthread' [-Wunused-command-line-argument]\r\n" |
| `text_encoding_errors` | F1 | AssertionError: b"clang: warning: argument unused during compilation: '-pthread' [-Wunused-command-line-argument]\r\n" |
| `text_encoding_crash_preservation` | F5 | FileNotFoundError: [WinError 2] The system cannot find the file specified |
| `lsp_text_encoding` | F3 | AssertionError: (1, b'') |
| `text_encoding_install` | F1 | AssertionError: b"clang: warning: argument unused during compilation: '-pthread' [-Wunused-command-line-argument]\r\n" |
| `log_execution` | F2 | AssertionError: (WindowsPath('D:/a/Void.Language/Void.Language/docs/log/evidence/run-rvys5brg'), ['D:\\a\\Void.Language\\Void.Language\\build\\Debug\\voidc.exe', 'run', 'D:\\a\\Void.Language\\Void.Language\\tests\\log\\smoke.void', '7', '--no-cach... |
| `log_oracles` | F6 | OSError: [Errno 22] Invalid argument |
| `log_cache` | F8 | FileNotFoundError: [WinError 3] The system cannot find the path specified: 'C:\\Users\\RUNNER~1\\AppData\\Local\\Temp\\VOID-log-cache-42x0zcd5\\.void\\cache\\v1' |
| `log_install` | F3 | AssertionError: (WindowsPath('D:/a/Void.Language/Void.Language/docs/log/evidence/run-n3eoi39u'), ['C:\\hostedtoolcache\\windows\\Python\\3.12.10\\x64\\python3.exe', 'D:\\a\\Void.Language\\Void.Language\\tests\\log\\lsp.py', 'C:\\Users\\RUNNER~1\\A... |
| `log_console` | F9 | subprocess.TimeoutExpired: Command '['D:\\a\\Void.Language\\Void.Language\\build\\Debug\\voidc.exe', 'run', 'C:\\Users\\RUNNER~1\\AppData\\Local\\Temp\\VOID-log-console-xl17nrgg\\main.void', '--no-cache', '--no-opt']' timed out after 600 seconds |
| `lsp_log` | F3 | UnicodeEncodeError: 'charmap' codec can't encode character '\U0001f525' in position 58: character maps to <undefined> |
| `collections_stdlib_host` | F8 | AssertionError: (WindowsPath('D:/a/Void.Language/Void.Language/docs/collections/evidence/run-34bffj7c'), ['D:\\a\\Void.Language\\Void.Language\\build\\Debug\\void_collections_stdlib_host.exe', 'D:\\a\\Void.Language\\Void.Language', 'C:\\Users\\RUN... |
| `collections_stdlib_execution` | F2 | AssertionError: (WindowsPath('D:/a/Void.Language/Void.Language/docs/collections/evidence/run-olang9v1'), ['D:\\a\\Void.Language\\Void.Language\\build\\Debug\\voidc.exe', 'run', 'D:\\a\\Void.Language\\Void.Language\\tests\\collections_stdlib\\seman... |
| `collections_stdlib_failures` | F2 | AssertionError: (WindowsPath('D:/a/Void.Language/Void.Language/docs/collections/evidence/run-63p7ctqd'), ['D:\\a\\Void.Language\\Void.Language\\build\\Debug\\voidc.exe', 'run', 'D:\\a\\Void.Language\\Void.Language\\tests\\collections_stdlib\\mutat... |
| `lsp_collections_stdlib` | F3 | UnicodeEncodeError: 'charmap' codec can't encode character '\U0001f525' in position 53: character maps to <undefined> |
| `collections_stdlib_install` | F3 | AssertionError: (WindowsPath('D:/a/Void.Language/Void.Language/docs/collections/evidence/run-rpzbgi6c'), ['C:\\hostedtoolcache\\windows\\Python\\3.12.10\\x64\\python3.exe', 'D:\\a\\Void.Language\\Void.Language\\tests\\collections_stdlib\\lsp.py', ... |
| `collections_stdlib_cache` | F8 | AssertionError: automatic cache did not publish once |
| `channel_host` | F2 | AssertionError: (WindowsPath('D:/a/Void.Language/Void.Language/docs/channel/evidence/implementation/run-utoh759v'), b'7\r\n', b'7\n') |
| `channel_execution` | F3 / F2 | UnicodeDecodeError: 'charmap' codec can't decode byte 0x81 in position 1359: character maps to <undefined> |
| `io_execution` | F2 | AssertionError: ([WindowsPath('D:/a/Void.Language/Void.Language/build/Debug/voidc.exe'), 'check', WindowsPath('C:/Users/RUNNER~1/AppData/Local/Temp/VOID io source 9q_9y27q/main.void')], b'Source checked successfully\r\n', b'Source checked successf... |
| `io_crash_preservation` | F4 | AssertionError |
| `io_async_execution` | F2 | AssertionError: ([WindowsPath('D:/a/Void.Language/Void.Language/build/Debug/voidc.exe'), 'check', WindowsPath('C:/Users/RUNNER~1/AppData/Local/Temp/VOID io async wire jv1k_c4p/main.void')], b'Source checked successfully\r\n', b'Source checked succ... |
| `io_examples_install` | F1 | AssertionError: b"clang: warning: argument unused during compilation: '-pthread' [-Wunused-command-line-argument]\r\n" |
| `async_syntax_aot_values_normal` | F2 | AssertionError: (['D:\\a\\Void.Language\\Void.Language\\build\\Debug\\voidc.exe', 'check', 'D:\\a\\Void.Language\\Void.Language\\tests\\async\\syntax\\values.void'], b'Source checked successfully\r\n', b'Source checked successfully\n', b'') |
| `async_syntax_aot_values_optimized` | F2 | AssertionError: (['D:\\a\\Void.Language\\Void.Language\\build\\Debug\\voidc.exe', 'check', 'D:\\a\\Void.Language\\Void.Language\\tests\\async\\syntax\\values.void'], b'Source checked successfully\r\n', b'Source checked successfully\n', b'') |
| `async_syntax_aot_errors_normal` | F2 | AssertionError: (['D:\\a\\Void.Language\\Void.Language\\build\\Debug\\voidc.exe', 'check', 'D:\\a\\Void.Language\\Void.Language\\tests\\async\\syntax\\errors.void'], b'Source checked successfully\r\n', b'Source checked successfully\n', b'') |
| `async_syntax_aot_errors_optimized` | F2 | AssertionError: (['D:\\a\\Void.Language\\Void.Language\\build\\Debug\\voidc.exe', 'check', 'D:\\a\\Void.Language\\Void.Language\\tests\\async\\syntax\\errors.void'], b'Source checked successfully\r\n', b'Source checked successfully\n', b'') |
| `async_syntax_aot_cancelled_normal` | F2 | AssertionError: (['D:\\a\\Void.Language\\Void.Language\\build\\Debug\\voidc.exe', 'check', 'D:\\a\\Void.Language\\Void.Language\\tests\\async\\syntax\\cancelled.void'], b'Source checked successfully\r\n', b'Source checked successfully\n', b'') |
| `async_syntax_aot_cancelled_optimized` | F2 | AssertionError: (['D:\\a\\Void.Language\\Void.Language\\build\\Debug\\voidc.exe', 'check', 'D:\\a\\Void.Language\\Void.Language\\tests\\async\\syntax\\cancelled.void'], b'Source checked successfully\r\n', b'Source checked successfully\n', b'') |
| `async_syntax_aot_failure_normal` | F2 | AssertionError: (['D:\\a\\Void.Language\\Void.Language\\build\\Debug\\voidc.exe', 'check', 'D:\\a\\Void.Language\\Void.Language\\tests\\async\\syntax\\failure.void'], b'Source checked successfully\r\n', b'Source checked successfully\n', b'') |
| `async_syntax_aot_failure_optimized` | F2 | AssertionError: (['D:\\a\\Void.Language\\Void.Language\\build\\Debug\\voidc.exe', 'check', 'D:\\a\\Void.Language\\Void.Language\\tests\\async\\syntax\\failure.void'], b'Source checked successfully\r\n', b'Source checked successfully\n', b'') |
| `async_syntax_aot_spawn_normal` | F2 | AssertionError: (['D:\\a\\Void.Language\\Void.Language\\build\\Debug\\voidc.exe', 'check', 'D:\\a\\Void.Language\\Void.Language\\tests\\async\\syntax\\spawn.void'], b'Source checked successfully\r\n', b'Source checked successfully\n', b'') |
| `async_syntax_aot_spawn_optimized` | F2 | AssertionError: (['D:\\a\\Void.Language\\Void.Language\\build\\Debug\\voidc.exe', 'check', 'D:\\a\\Void.Language\\Void.Language\\tests\\async\\syntax\\spawn.void'], b'Source checked successfully\r\n', b'Source checked successfully\n', b'') |

Full original errors: [Debug](evidence/native-ci-40861888611/Windows-X64-Debug.md), [Release](evidence/native-ci-40861888611/Windows-X64-Release.md).

### macOS-ARM64 (both Debug and Release)

| Failed test | Family | Original exception/assertion (full output linked) |
| --- | --- | --- |
| `json_preservation` | F11 / F4 | StopIteration |
| `math_preservation` | F4 | AssertionError |
| `random_preservation` | F4 | StopIteration |
| `lsp_const_recovery_stack` | F7 | ValueError: current limit exceeds maximum limit |
| `text_encoding_crash_preservation` | F5 | FileNotFoundError: [Errno 2] No such file or directory: '../../../../../../Library/Frameworks/Python.framework/Versions/3.12/bin/python3.12' |
| `lsp_text_encoding_stack` | F7 | ValueError: current limit exceeds maximum limit |
| `log_install` | F7 | AssertionError: (PosixPath('/Users/runner/work/Void.Language/Void.Language/docs/log/evidence/run-8spmfr1r'), ['/Library/Frameworks/Python.framework/Versions/3.12/bin/python3.12', '/Users/runner/work/Void.Language/Void.Language/tests/log/lsp.py', '... |
| `lsp_log` | F7 | ValueError: current limit exceeds maximum limit |
| `collections_stdlib_install` | F4 | Installed stdlib definition request: `-32602`, `Import escapes the project root` from `<document identity>`, because the URI uses `/var` while the loaded module records `/private/var`. |
| `io_crash_preservation` | F4 | AssertionError |
| `lsp_io_stack` | F7 | ValueError: current limit exceeds maximum limit |
| `io_kernel` | F10 | invalid native filename escaped UTF-8 validation |

Full original errors: [Debug](evidence/native-ci-40861888611/macOS-ARM64-Debug.md), [Release](evidence/native-ci-40861888611/macOS-ARM64-Release.md).

## Sequential repair checkpoint

Inventory complete. All feasible source repairs are implemented. Full Release,
focused Debug and sanitizer checks passed. Native confirmation is pending for
all families. F10 requires the new native creation/errno observation before the
original macOS filesystem incident can be closed.

No cross-platform native result will be inferred from Linux-only checks.

## Corrections and verification by root cause

The changes below follow the inventory rather than the order in which a hosted
job happened to report its first exception. No tests are removed or marked skipped.
The original four inventories above remain the acceptance checklist.

| Family | Correction | Focused Linux result | Native boundary |
| --- | --- | --- | --- |
| F1 | `cmake/NativeBuild.cmake` appends `-pthread` only on Unix. All Windows static CRT/native libraries and Apple libc++/SDK flags remain. | Configure/ORC passed; `binary_execution`, three worker/owner tests and `io_execution`: 5/5. CMake generation of all three platform manifests also checked, including absence of Windows `-pthread`. | Manifest generation under simulated platform variables is not Windows linking. Require native warning-free AOT links. |
| F2 | `tools/test_platform.py::native_text` constructs expected native text bytes only. CLI check messages, runner summaries and returned integers use it explicitly. Mixed Math/log output retains every LF runtime/log byte and changes only the CRT integer suffix. Collections `authority`, examples, file streams, wire bytes and diagnostics keep their original exact bytes. Channel's first three fixtures return integers; its generated/numeric/example fixtures print through VOID and retain LF. | 16/16 status/worker/AOT/collections/log checks passed; channel also passes in F3. | Native CRLF expectations require Windows confirmation. Captured output is never normalized. |
| F3 | Explicit UTF-8 for affected generated source writes, source overlay reads and native-manifest reads in OS, text/encoding, log, collections, channel and shared Math/worker helpers. Unicode literals and negative assertions remain. | Six affected CTests passed. The controlled cp1252 probe reproduces all six baseline encode/decode failures, then executes the corrected scripts with all observed source/manifest text accesses explicitly UTF-8. | The probe emulates file defaults on Linux; it does not emulate CreateProcess, Windows filesystem APIs or native text mode. |
| F4 | Receipt assertions use resolved source/cwd identities. The shared recorder canonicalizes its source set to prevent alias duplicates while retaining lexical command argv. Diagnostics compare canonical generic UTF-8 paths. Collections' installed stdlib definition request uses the loaded module's physical path. | Six path/preservation CTests passed; original source bytes, read-only copies, hashes, streams, provenance and command assertions retained. Controlled POSIX ancestor-alias reproduction verifies canonical deduplication, launch bytes after edits, colliding basenames and exact lexical argv. The baseline collections LSP request fails through a stdlib symlink; the corrected request passes. Existing `lsp_stage8` escape/descendant-alias rejection passes. | Windows 8.3 and macOS `/var` aliases still require native execution. No production VFS authorization policy changed. |
| F5 | Crash fixture uses the absolute selected Python interpreter. Windows CreateProcess and Darwin symlink traversal must not reinterpret a relative executable. Synthetic expected raw streams use `os.write`, retaining exact LF/NUL/non-UTF-8 data instead of relying on Python's native text newline. | `text_encoding_crash_preservation` passed: all 13 intentional incidents, abnormal/reserved statuses, source edits, read-only collisions, environment inheritance and ordinary rejection checks remain. Exact selected interpreter argv/hash are asserted. | Native Windows exception statuses and launch remain pending. |
| F6 | Historical timestamp oracle uses integer UTC epoch plus `timedelta`, avoiding platform C `fromtimestamp` range restrictions. | `log_oracles` passed, including all eight original instants, year 0001/year 9999, pre-epoch, leap/day/year boundaries and nanosecond-to-millisecond framing. | Run unchanged cases on Windows; no historical case was removed. |
| F7 | Hard-stack LSP enforcement moves from the running Python launcher to the native child's `preexec_fn`, matching the existing successful compiler fixture boundary. `small_stack` queries inherited limits, sets both limits to exactly 1048576, checks readback and records `VOID_TEST_STACK` JSON on captured stderr. Failed setup still fails the test. | Seven stack/install/alias CTests passed. Explicit Linux readback: inherited `[8388608, -1]`, observed `[1048576, 1048576]`; parent remains unchanged. | Original native logs did not record `getrlimit`. Python's EINVAL-derived “current limit exceeds maximum” text does not establish the old hard maximum. Collect the new exact macOS limits before closure; no macOS limit value is invented. |
| F8 | Logging/collections fixtures incorrectly assumed persistent cache availability. At the baseline, `NativeCache` intentionally has no Windows publisher until ACL/path safety is verified. The repaired Windows host requires `unavailable` on both compiles and no `.void` directory. CLI fixtures execute all retries/bypasses and require fresh state plus no persistent directory. Unix still requires one publication per mode, unchanged hit bytes/mtime and no bypass publication. | `log_cache`, `collections_stdlib_host`, `collections_stdlib_cache`: 3/3. All GC/allocation/publication sweeps, compatible/incompatible reloads, captures and active-generation lease checks still execute. | No Windows cache implementation or ACL fallback was introduced. Native Windows must demonstrate strict unavailable behavior and fresh Runtime state. Persistent Windows caching remains an existing unsupported capability, not a repair claim. |
| F9 | Production Windows console maps `ERROR_NO_DATA` on write to `broken_pipe`; reads retain their empty/nonblocking semantics. Existing retry logic previously retried this permanent write failure indefinitely. New real Win32 anonymous/synchronous-named/overlapped-named closed-peer tests require zero progress, native error code, `broken_pipe` before a 1-second deadline and an open borrowed writer after Runtime cleanup. | `log_console` and 11 console neighbors: 12/12. Existing POSIX closed-peer/SIGPIPE, binary transfer, cancellation and endpoint tests pass. | The Win32 diagnosis follows the code's retry path and documented OS semantics; actual failing syscall/error number was not traced in the archived timeout. Require the new native closed-peer tests and original log-console JIT/O0/O2/AOT file-independence checks. |
| F10 | Replace unchecked invalid-name `ofstream` creation with raw checked `open`, captured errno and exact-byte `lstat`. If creation succeeds, retain the original mandatory enumeration rejection. On Apple only, a creation failure must be exactly `EILSEQ`, lookup must reject the same bytes, and valid directory enumeration must succeed. All hosts must also reject the malformed input path before OS lookup. No production filesystem validation changed. | `io_kernel`, `io_execution`, `io_errors_source`, `io_crash_preservation`: 4/4. Linux fixture reports `created=1 errno=0`, and the actual malformed directory entry is rejected. | **Native observation required.** Original CI never checked fixture creation, so it does not prove that an invalid entry existed. Apple documents that APFS rejects malformed UTF-8 at creation. The new native errno/creation record must confirm the branch; unexpected errno or success with escaped invalid entry remains a failing test. |
| F11 | `json_preservation` now registers `tests/json/preservation.py` with compiler/profile arguments. The new fixture exercises the actual JSON adapter with ordinary rejection, reserved ASan/UBSan exits and timeout; validates JSON/input launch snapshots, bytes/hashes, command, cwd, executable and adapter provenance. | JSON plus all four existing preservation fixtures: 5/5. The test no longer accidentally executes Random's unrelated adapter. | JSON native macOS execution remains pending; registration stays Unix as before. |

Completed local commands and results follow. Logs are retained in this record's evidence directory.

## Primary platform references

- Apple's [APFS filename contract](https://developer.apple.com/library/archive/documentation/FileManagement/Conceptual/APFS_Guide/FAQ/FAQ.html) requires valid UTF-8 for filename creation. This supports checking fixture creation first; it does not supply the missing syscall result from this runner.
- Apple's [XNU stack-limit implementation](https://github.com/apple-oss-distributions/xnu/blob/main/bsd/kern/kern_resource.c) can reject shrinking a running stack, including custom/non-main stacks and current usage beyond the proposed boundary. The generic Python exception alone cannot identify which condition occurred. Enforcing the native child before exec avoids modifying the launcher's running stack.
- Microsoft's [pipe wait modes](https://learn.microsoft.com/en-us/windows/win32/ipc/named-pipe-type-read-and-wait-modes) distinguish empty nonblocking reads from writes; [error code 232](https://learn.microsoft.com/en-us/windows/win32/debug/system-error-codes--0-499-) identifies a pipe being closed. Treating a permanent write failure as transient explains the observed unbounded retry path; native trace confirmation remains required.

## Recovery checkpoint

All eleven families have been investigated and feasible corrections are present.
Local verification and reconciliation are complete. No hosted CI, staging,
commit, push or branch operation was performed. Stop for independent Golden
Wrench source review. F10 and the Win32-specific F9 test body require native
execution for their final verdict; they are not marked native passes.

## Completed local verification

Linux x86-64, Python 3.14.8, official LLVM/Clang/LLD 23.1.4 and existing Unix
Makefiles builds. Exact host/tool versions and protected-input hashes are in
[`source-audit.json`](evidence/native-ci-40861888611/source-audit.json).
No dependency version or runtime package was introduced. Existing Windows zlib
1.3.2/zstd 1.5.7/LibXml2 2.15.4 `/MT`, DIA and Apple vendor libc++/system-runtime
selection remain as recorded in the configuration repair.

| Selection | Result | Elapsed | Evidence |
| --- | --- | --- | --- |
| Full Release, all registered tests | 369/369; zero failures/skips | 530.50 s | [log](evidence/native-ci-40861888611/local-release.txt), [JUnit](evidence/native-ci-40861888611/local-release.xml) |
| Focused Debug, all cause families and neighbors | 26/26; zero failures/skips | 399.99 s | [log](evidence/native-ci-40861888611/local-debug.txt), [JUnit](evidence/native-ci-40861888611/local-debug.xml) |
| Focused ASan/UBSan with fatal findings | 11/11; zero failures/skips | 165.64 s | [log](evidence/native-ci-40861888611/local-strict.txt), [JUnit](evidence/native-ci-40861888611/local-strict.xml) |
| Final passing-output stack observation, Release / Debug | 7/7 in each; zero failures/skips | 26.17 s / 91.90 s | [Release](evidence/native-ci-40861888611/local-stack-final.xml), [Debug](evidence/native-ci-40861888611/local-stack-final-debug.xml) |
| Controlled baseline/fixed platform probes | Passed | See output | [reproducer](evidence/native-ci-40861888611/platform_checks.py), [results](evidence/native-ci-40861888611/platform-checks-final.txt) |

The final stack-reporting addition was made after the full run, then the seven
affected LSP/install/alias checks were rerun in each configuration. It verifies
the child's limit report exists and retains it in passing CTest output. The
Win32-only closed-peer fixture also prints its actual error code for native
review; that body cannot be compiled or executed by these Linux runs.

The full Release run contains all **63 distinct original failed test names**.
[`verification.json`](evidence/native-ci-40861888611/verification.json) reconciles
each of the 70 platform/name entries (58 Windows + 12 macOS) with its family,
Linux result and explicitly pending native status. The focused sanitizer
selection contains 11 registered tests: the existing CMake policy does not
register the three requested hard-stack LSP names in sanitizer builds. No
registration or sanitizer policy was changed by this repair.

Commands used (the individual family results are also retained in
`focused-f*.txt`):

```sh
cmake -S . -B build/llvm23-release
cmake --build build/llvm23-release -j 4
ctest --test-dir build/llvm23-release -j 4 --output-on-failure \
  --output-junit /tmp/void-burndown-release.xml

cmake -S . -B build/llvm23-debug
cmake --build build/llvm23-debug --target voidc voidls void_io_runner \
  void_io_kernel void_runtime_console_transfers void_collections_stdlib_host \
  void_stop_token_probe -j 3
ctest --test-dir build/llvm23-debug -j 2 --output-on-failure \
  -R '^(toolchain_stop_token|math_frontend|math_errors|math_preservation|os_errors|lsp_os|random_errors|random_preservation|text_encoding_crash_preservation|lsp_text_encoding_stack|json_preservation|lsp_const_recovery_stack|log_console|log_cache|log_install|lsp_log|collections_stdlib_host|collections_stdlib_cache|collections_stdlib_install|lsp_collections_stdlib|channel_execution|io_kernel|io_execution|io_crash_preservation|lsp_io_stack|runtime_console_transfers)$' \
  --output-junit /tmp/void-burndown-debug.xml

cmake -S . -B build/llvm23-strict
cmake --build build/llvm23-strict --target voidc voidls void_io_runner \
  void_io_kernel void_runtime_console_transfers void_collections_stdlib_host \
  void_stop_token_probe -j 3
ASAN_OPTIONS=halt_on_error=1:abort_on_error=0:exitcode=87:detect_leaks=1 \
UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1:exitcode=86 \
ctest --test-dir build/llvm23-strict -j 2 --output-on-failure \
  -R '^(toolchain_stop_token|runtime_console_transfers|io_kernel|io_execution|io_crash_preservation|text_encoding_crash_preservation|json_preservation|lsp_const_recovery_stack|lsp_text_encoding_stack|lsp_io_stack|log_console|collections_stdlib_host|collections_stdlib_cache|log_cache)$' \
  --output-junit /tmp/void-burndown-strict.xml

python3 -u docs/llvm23/evidence/native-ci-40861888611/platform_checks.py \
  build/llvm23-release
# Final reporting rerun, once for each of llvm23-release and llvm23-debug:
ctest --test-dir build/llvm23-release -j 2 --output-on-failure \
  -R '^(lsp_const_recovery_stack|lsp_text_encoding_stack|lsp_io_stack|lsp_log|log_install|collections_stdlib_install|lsp_stage8)$' \
  --output-junit /tmp/void-burndown-stack-final.xml
git diff --check
```

All three configuration runs re-executed and passed `VOID_LLVM_ORC_RUNS`;
`toolchain_stop_token` executed in each selected profile. The original native
job logs also show ORC success in all four jobs and
`VOID_HAS_STANDARD_STOP_TOKEN - Success` in both macOS jobs. Those native results
belong to the baseline, not this uncommitted repair.

The cp1252 control uses `PYTHONUTF8=0` in isolated child processes, changes only
the default encoding for VOID/manifest text opens, reproduces baseline failures,
and executes corrected actual scripts without substituting compiler results.
The alias control reproduces the baseline installed-stdlib definition rejection
before the corrected request. CMake manifest checks simulate platform variables
for generation only. These controls do not emulate native runtimes.

Local fixture receipts were retained without byte changes under
`.toolchains/burndown-40861888611/generated-evidence/` (12,352 generated files).
`generated-relocations.json` maps original absolute locations to retained copies
and hashes; receipt contents are unchanged. Two tracked OS deleted-image
receipts rewritten by the full suite were archived there and restored to their
baseline bytes. Historical evidence is otherwise untouched. The complete four
downloaded native logs/artifacts remain in the ignored inspection root;
reviewable original failure blocks and concise local results are in this
document's evidence directory.

## Modified files and preserved boundaries

The complete changed-path/hash inventory is
[`source-audit.json`](evidence/native-ci-40861888611/source-audit.json).

- Build integration: `CMakeLists.txt`, `cmake/NativeBuild.cmake`.
- Production: `src/runtime/console.cpp`, solely Windows write-error classification.
- Shared test infrastructure: new `tools/test_platform.py`, `tools/lsp_client.py`,
  `tests/random/harness.py`, `tests/text_encoding/crash.py`, and log/collections/channel support helpers.
- Fixture repairs: Math frontend/errors/install/worker/preservation/bootstrap;
  OS errors/LSP/install/owner; Random errors/preservation/worker; text/encoding
  LSP/crash test; log execution/LSP/oracles/cache/install; collections
  execution/failures/frontend/LSP/host/cache; channel execution/host;
  IO execution/LSP/kernel/preservation; async syntax AOT; const-recovery LSP;
  Windows native console transfer fixtures; new `tests/json/preservation.py`.
- Engineering records: this inventory/evidence, `IMPLEMENTATION.md`,
  `COMPATIBILITY.md`, `ACCEPTANCE.md` under `docs/llvm23/`.

Root README, official LLVM pin/SDK installer, dependency bootstrap, DIA setup,
Apple pairing/probes, entropy implementation and LLVM backend/O2 pipeline have
matching baseline/current hashes. No frontend, IR, precise GC, generation lease,
production filesystem or Runtime ABI change is made. HEAD remains the authorized
baseline and the staged diff is empty.

## Exact recommended Boss-controlled native acceptance

After independent source review, rerun the existing seven-job link-experiment
matrix with the same configuration/variant and LLVM 23.1.4 SDKs. This sprint did
not dispatch it. On each Windows x64 and macOS ARM64 Debug/Release job:

1. Retain SDK/compiler/linker/dependency identity, complete configure log and
   native manifest. Require executed ORC configuration success. macOS must also
   retain `VOID_HAS_STANDARD_STOP_TOKEN` compile/link/run success using the
   selected vendor headers/system libc++ and `toolchain_stop_token` execution.
2. Complete the ordinary native build. Inspect `void-native.txt`: Windows must
   have no `-pthread` and must retain static CRT/native library flags; Apple must
   retain vendor include/sysroot/framework flags and Unix threading.
3. Run the full native suite, with no selection, exclusions or retries hiding
   original failures:

   ```sh
   cmake --build build/Debug --parallel 3
   ctest --test-dir build/Debug --output-on-failure --output-junit ctest-Debug.xml
   cmake --build build/Release --parallel 3
   ctest --test-dir build/Release --output-on-failure --output-junit ctest-Release.xml
   ```

4. Windows: retain verbose `runtime_console_transfers` and `log_console` output.
   All three new closed-peer shapes must report `broken_pipe`, zero progress,
   native error code and open borrowed writer, rather than deadline expiry.
   Require original log file-independence/no-replay assertions in JIT/O0/O2 and
   linked AOT. Require Unicode source/LSP/install/channel tests, canonical crash
   receipts and warning-strict AOT siblings. Cache host must report `unavailable`;
   no persistent directory may be created, and cold-state/reload/GC/allocation/
   lease checks must still run.
5. macOS: retain verbose output/JUnit for `io_kernel`, the three LSP stack tests
   and `lsp_log`. Record actual `created`/`errno` from raw filename creation and
   `VOID_TEST_STACK` inherited/effective pairs. If creation succeeds, enumeration
   rejection is mandatory; if the filesystem rejects it, the exact native
   rejection branch and VOID invalid-input rejection must both pass. Effective
   soft and hard limits must each be 1048576. Require JSON and all other
   preservation checks and relocated collections/log LSP tests.
6. Compare both native configurations against all 70 platform/name entries in
   `verification.json`; retain complete LastTest.log/CTest XML and any original
   crash receipts before retry. All three Ubuntu jobs, including sanitizers,
   must remain green.

Useful verbose gate on each native build directory, in addition to the full suite:

```sh
ctest --test-dir build/Debug -V \
  -R '^(toolchain_stop_token|runtime_console_transfers|log_console|log_cache|collections_stdlib_host|collections_stdlib_cache|io_kernel|lsp_const_recovery_stack|lsp_text_encoding_stack|lsp_io_stack|lsp_log|log_install|collections_stdlib_install|json_preservation|text_encoding_crash_preservation|io_crash_preservation|math_preservation|random_preservation)$'
```

Windows does not register the Unix-only stack/JSON fixture names; the complete
native suite and its original platform inventory remain authoritative. If the
new native observations contradict the diagnosed F7/F9/F10 branch, retain that
evidence and investigate it; Linux results do not close those platform incidents.
