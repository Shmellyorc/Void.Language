# G65 — Native approval reuse in VS Code

Baseline `2e10af44aec61c6b9ff5608c8e406593398840cb` (G64).
Candidate branch `codex/g65-editor-native-approval-reuse`.
Recommendation **INTEGRATE**.

## Developer workflow

```sh
voidc new app --name MyGame
cd MyGame
voidc add raylib
voidc run .
```

Run the last command in an ordinary interactive terminal. Review the provider
shown by VOID and type `approve` to save consent. Open the project in a
trusted VS Code workspace with the matching VOID extension and compiler.
**VOID: Run Project**, **VOID: Build Project**, and **F5** reuse that consent
with saved project sources. F5 runs G64's Debug build before launching through
the configured platform adapter.

Use `voidc native approvals` to list approval IDs and
`voidc native revoke APPROVAL_ID` to remove one. After an intentional provider
or application identity change, verify the package and lock, then run
`voidc run .` in the terminal to review the new identity. Installation,
template creation, editor analysis and compilation grant no consent.

## Authorization and verification

The VS Code commands pass the internal `--reuse-native-approval` argument
to manifest-selected `voidc run` or `voidc build`. The compiler accepts
that argument only for those operations and rejects combining it with
`--allow-native`. The extension adds it after the existing trusted-workspace
and selected-manifest gates, and verifies the selection and saved-source
requirements before spawning the compiler. Check and Package tasks keep
their existing authorization behavior.

The reuse path looks up the existing exact G63 record under the approval
store's lock. A miss returns status 2 with the command for terminal review.
A malformed record or store error fails with its diagnostic and leaves the
record bytes in place. Reuse has no prompt, pending approval, or approval
publication path. The successful reuse tests compare both the record bytes
and modification time. The compiler continues to restrict normal redirected
Run and Build, and rejects both durable modes when `CI` is set.
`--allow-native` continues to authorize one deliberate invocation.

G63's approval digest still covers the physical project root and its
identity, project and application IDs, selected entry path, verified
dependency/lock and catalog identities, provider target and ABI, descriptor
and runtime payload identity, binding identity and bytes, and compiler build
identity. A record saved by a different compiler build needs fresh terminal
approval. Edits to ordinary application logic can retain approval when those
identities stay the same. Reuse applies to the managed native catalogs
admitted by G63.
Explicit CLI authorization remains available for the other native workflows.

The compiler still verifies locked dependencies, captures the selected
source graph again, and verifies payloads via the existing private JIT and
checked AOT paths. Bad catalog, source, artifact or lock bytes retain their
specific verification failures. A correctly formed provider with a different
identity reaches approval matching and is refused until approved. G64's
prelaunch task fails on these denials, keeping an older executable on disk
without launching it. Revocation affects subsequent authorization checks.
An independently linked program already on disk can still be launched
directly outside the compiler.

## Qualification

The focused compiler tests cover independent C-provider execution under
Debug and Release JIT and separately linked AOT, approved and unapproved
use, a copied project and a changed entry, redirected ordinary CLI use,
CI with an existing record, deliberate `--allow-native` in CI, malformed
records, revocation, and tampered catalog, target, descriptor, library,
physical binding, native source and lock. They also exercise terminal decline,
a catalog change during the prompt, explicit consent after a legitimate new
lock, and ordinary package and template workflows.

Linux Release and fatal ASan/UBSan/LeakSanitizer each passed 7/7 selected
tests with LLVM/Clang/LLD 23.1.4. The suites included project creation,
creation through the CLI, profiles, native debug info, project sessions,
templates and the package/approval lifecycle. The VS Code extension
TypeScript build and 8/8 configuration tests passed.

Native Windows 10 x64 Debug and Release each passed 6/6 selected tests
with LLVM/Clang/LLD 23.1.4. Both profiles passed the 1 MiB stack-reserve
checks for `voidc`, `voidls`, the project creation test executable and
the independent native fixture. The Windows receipts reverify the
3,357-file build input and the installed compiler and provider binaries.
The editor test bundle identity matches the Linux bundle.

Actual VS Code 1.141.0 ran the bundled raylib fixture through all three
editor workflows on Linux and Windows 10 x64 build 19045. Linux used LLVM
LLDB DAP 0.7.20261117 and Windows used Microsoft C/C++ 1.35.6 with GDB
17.2. Each host used an isolated profile and approval store and provided
consent through a real POSIX PTY or Windows ConPTY. The test invokes the
generated launch configuration through VS Code's debug API, which runs the
same prelaunch task as F5. The fixture independently checks a pixel value
of 241 and returns exit 17. The following results match on both hosts.

| Approval state | Run task exit | Build task exit | F5 Debug build exit | Debug adapter launch |
| --- | ---: | ---: | ---: | --- |
| Missing | 2 | 2 | 2 | No |
| Exact record | 17 | 0 | 0 | Yes |
| Valid catalog with changed bytes | 2 | 2 | 2 | No |
| Revoked | 2 | 2 | 2 | No |

The editor host verifies there is no initial executable, compares the
previous executable bytes across the later denials, and independently runs
the Debug AOT executable. It removes the fixture's result file before F5
and verifies that the debuggee writes a new one after the native pixel
check. All later denials leave the result absent. It confirms the same
saved approval listing after Run, Build, and F5. The fixture uses the
generated starter and existing task commands without injected native
permission settings.

The Windows adapter reported exit 21 while the independently run
executable returned 17, matching the G64 limitation. The debugger launch
also wrote the pixel receipt, so the exit-status discrepancy does not
determine whether native code ran.

The existing non-native starter editor regression also passed on Linux
under G65, including failed-build protection, a source breakpoint, stepping
and scalar inspection. Direct standalone `.void` execution printed its
expected output.

An additional instrumented VS Code run initially hit LeakSanitizer's
failure under LLDB. The identical fixture reproduced a LeakSanitizer
abort under command-line LLDB after printing the correct pixel. The
instrumented editor replay leaves fatal ASan/UBSan/LSan enabled for the
compiler, JIT, and separately executed AOT control, and disables leak
detection only in the process being traced by LLDB. That split replay
passed Run, Build, F5 and the independent AOT oracle. [LLVM compiler-rt
and LLDB issue](https://github.com/llvm/llvm-project/issues/203853)

The Windows replay uses the filesystem's canonical executable path for
its independent output oracle. The same native EXE returned 17 when
launched with `C:\` and hit the existing payload-path alias rejection
when the probe launched it with `c:\`. The editor debugger's selected
executable already uses the canonical spelling. VOID's native loader
checks stay in place. [Win32 module path behavior](https://learn.microsoft.com/en-us/windows/win32/api/libloaderapi/nf-libloaderapi-getmodulefilenamew)

One earlier Windows replay wrote the fixture result inside the project
and invoked F5 immediately after a separate Build. The existing G64
project-state check refused the launch during a file-watcher update.
The final fixture writes its result outside the watched project and
launches F5 from a fresh build state. This qualification covers those
ordinary workflows; it does not qualify overlapping builds or edits.

## Scope and replay

The change touches the compiler CLI and approval gate, VS Code invocation,
focused regressions and documentation. It adds no dependency, Runtime API,
persistent cache or debugger adapter. The inherited VS Code requirement for
a trusted saved project, the G64 Windows adapter exit-status limitation
and the existing G63 compiler-build binding still apply. No startup or
throughput benchmarks were run for this focused change.

The review packet supplies the exact binary-capable patch, SHA-256
postimages, test receipts and editor runner. The runner creates a starter,
installs the bundled raylib package, and invokes the compiled
`nativeApprovalHost` extension test. It uses the isolated test-only
`approval_console.py` driver for POSIX PTY or Windows ConPTY consent.
