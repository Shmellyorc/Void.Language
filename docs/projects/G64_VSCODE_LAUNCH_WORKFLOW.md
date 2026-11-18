# G64 — VS Code build before launch

Baseline: G63 `27b8ae4e9368c8a7c046fd05c07ff0815503a2f3`.
Candidate: `codex/g64-vscode-launch-workflow`.

On Linux or Windows x64, run `voidc new app --name MyGame`, open
**MyGame** as a trusted VS Code folder, and install the matching VOID
extension. On Linux use LLVM's official LLDB DAP extension and point
`lldb-dap.executable-path` at LLVM 23.1.4. On Windows use Microsoft's
C/C++ extension and put native GDB 17.2 (`gdb.exe`) on PATH. Select
**VOID Debug project** and press **F5**.
The saved application builds in Debug and launches with the starter's
full native debug information. An open source tab and a pre-existing
executable are not prerequisites.

The generated launch configuration uses VS Code's documented
[preLaunchTask](https://code.visualstudio.com/docs/debugtest/debugging-configuration)
and an extension task resolved by the existing project command path.
The task selects the same project, compiler, optional entry override,
and build output as **VOID: Build Project**, with an explicit Debug
profile. It uses the existing process runner and compiler problem
matcher. Ordinary commands still honor `void.tasks.profile`. There is
no other compiler or debugger implementation.

VS Code substitutes command variables before the task runs. The launch
can therefore resolve the selected program path before it exists. Once
the task has completed, VOID checks that the selected program and working
directory still agree with the launch and that the executable exists.
The workspace uses `debug.onTaskErrors: abort` because VS Code can
otherwise offer **Debug Anyway** when a task fails. The managed launch
requires `abort` or `showErrors`. The task reports preparation/spawn and
compiler errors as failed exits. The test retains the exact old binary,
makes the build directory unwritable, and observes no debugger launch
after that failure. A saved edit later reaches LLDB with the rebuilt
image. Stop and start F5 again after changing the project or entry during
an existing debug session. G62's manual launch without a prelaunch task
still requires a prebuilt executable.

G63 native trust is unchanged. The task child uses pipes and supplies
no `--allow-native`. A task cannot grant or reuse a durable approval
under G63's terminal requirement. The rejection directs the developer
to an ordinary interactive terminal and `voidc run .` for verification
and consent. Explicit CLI authorization remains an intentional option
where needed. The Debug manifest setting can produce the existing
native-debug-info warning during JIT Run Project; it is nonfatal.

The starter generates launch files only on Linux x64 and Windows x64.
`classlib` and its `lib` alias have no app launch files. The template
registry, custom template contents, standalone execution, language,
native ABI, package resolver and G60 validation algorithm are unchanged.

## Windows adapter and limitations

LLVM 23.1.4 emits embedded DWARF in the MSVC-targeted PE image. Its
pinned Windows archive has no LLDB DAP binary. The starter uses
Microsoft's official [`cppdbg` GDB adapter](https://code.visualstudio.com/docs/cpp/launch-json-reference#customizing-gdb-or-lldb)
with GDB 17.2 found on PATH. It still uses the same VOID task and
executable discovery as Linux.

The first isolated Windows editor started in Restricted Mode and
withheld the C/C++ extension. The isolated trusted replay loaded
official C/C++ 1.35.6 in VS Code 1.141.0. The pilot stopped in VOID
source and stepped with GDB. The adapter reported exit 0 when GDB
observed exit 17. The final generated-starter replay reported 21 for the
same real status of 17. The standalone executable and Run Project both
returned 17. Treat the adapter's displayed exit code as advisory.
**VOID: Run Project** remains available without a debug adapter. No workspace trust preference is changed by the starter.

## Qualification

| Platform and build | Focused compiler tests | Actual VS Code |
| --- | --- | --- |
| Linux x64 Release | 7/7 | VS Code 1.141.0 and official LLDB DAP 0.7.20261117; clean first F5, failed compiler build with the old image preserved, then edited-source F5 |
| Linux x64 fatal ASan/UBSan/LeakSanitizer | 7/7 | Compiler security and creation controls; debugger exercised separately under Release |
| Windows 10 x64 build 19045 Release | 6/6 | VS Code 1.141.0 and official C/C++ 1.35.6 / GDB 17.2; clean first F5, a failed prelaunch task blocked the old image, then edited-source F5; Run Project returned 17 |
| Windows 10 x64 build 19045 Debug | 6/6 | Editor replay uses the Release compiler and a Debug application build |

The two generated launches used the real editor's named launch entry
point, which runs the same `preLaunchTask` as pressing F5. On each
host the build task statuses were `[0, 1, 0]`, and only the first and
third attempts reached the adapter. The Linux failure is a real compiler
publication failure (unwritable output directory). Windows induced a
task setup failure (missing configured compiler). Both retained the old
executable byte for byte. An explicit unsafe **Debug Anyway** setting
was refused. The rebuilt applications stopped at source lines 2 and 3,
exposed `observed = 14`, and returned 17 when run independently. The
ordinary task setting was Release during these checks, so the full
Debug information also verifies that F5 selected its own Debug profile.

The Windows test used a separate VS Code profile, with workspace trust
disabled **in that test process only** for the task-owned fixture. Its
initial Restricted Mode replay refused to run. The shipped extension
still requires a trusted folder. Windows Run Project was replayed with
the external debugger extension disabled. The Windows build tests also
reused the established private native staging root after the builder
rejected the guest's inherited temporary-directory permissions. The original command-line
GDB control independently hit the source breakpoint and read 14.

Linux editor checks: eight configuration tests, 37 ordinary project
checks, and the missing-adapter, manual Debug, and generated Debug
scenarios. The real raylib editor host retained package discovery,
completion, hover, semantic highlighting and unsaved diagnostics; both
piped native Run and Build returned status 2 and saved no approval.
The focused native tests cover G63 independent C-provider approvals
and JIT/AOT, built-in and custom projects, and standalone paths.
No broad performance campaign was run. Toolchain: LLVM/Clang/LLD
23.1.4. The Windows PE tools and independent executables retain
1,048,576-byte stack reserves. The Windows replay verified all 3,357
input files and found no changes to the pinned SDK or provider archives.

The exact patch, identity manifest and replay receipts are external
review artifacts. All 14 postimages are verified by restoring the
patch on the stated baseline. The worktree is unstaged and uncommitted.

**Recommendation: INTEGRATE.** Both generated F5 flows and failed-build
controls passed in actual VS Code. Keep the Windows adapter exit-status
limitation visible and use Run Project or the standalone executable
when an exact status is required.
