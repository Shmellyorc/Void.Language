# LLVM 23.1.4 Runtime boolean ABI and optimization metadata corrections

22 November 2026. Deliverable B; [legacy checks and optimization audit](LEGACY_CHECKS_AND_OPTIMIZATION_AUDIT.md) is deliverable A. Starting HEAD: `c6015735` (the accepted `9959ab72` compiler/debugger repair is retained). Changes remain uncommitted for Sacred Wrench review. No hosted CI or Git mutation was performed.

## Findings and exact correction

### Runtime boolean boundary

Pinned Clang 23.1.4 was invoked with header-free **C++** declarations and real calls, at O0/O2, for `x86_64-unknown-linux-gnu`, `x86_64-pc-windows-msvc` and `arm64-apple-macosx11.0.0`. All three emit `zeroext i1` for boolean returns and boolean parameters, on both declarations and call sites. The initial C oracle independently agrees. Windows differs for `i8`, not for `bool`; the earlier research warning against assuming Windows rules was appropriate, but the measured boolean result is now established.

The actual VOID backend previously emitted bare `i1` declarations/calls, including `vd_console_print`'s flush argument and `vd_string_equal`'s result. Successful baseline boolean execution is preserved: **no observable misexecution was reproduced**. This is correction of an established ABI declaration mismatch, not a claim to have reproduced or fixed a crash.

Upstream authority:

- [LLVM 23 ABI attribute agreement](https://github.com/llvm/llvm-project/blob/llvmorg-23.1.4/llvm/docs/LangRef.md#L1320): ABI attributes must agree between declaration/definition and call site; extension without an attribute is target-specific.
- [Clang Windows bool special case](https://github.com/llvm/llvm-project/blob/llvmorg-23.1.4/clang/lib/CodeGen/Targets/X86.cpp#L3483): bool is extended even though ordinary narrow Windows integers are not.
- [SysV classification](https://github.com/llvm/llvm-project/blob/llvmorg-23.1.4/clang/lib/CodeGen/Targets/X86.cpp#L2665), [Darwin classification](https://github.com/llvm/llvm-project/blob/llvmorg-23.1.4/clang/lib/CodeGen/Targets/AArch64.cpp#L427), [Clang attribute emission](https://github.com/llvm/llvm-project/blob/llvmorg-23.1.4/clang/lib/CodeGen/CGCall.cpp#L2954).

`src/backend/llvm/runtime_bool_abi.hpp` adds only `zeroext` to boolean positions on external `vd_` Runtime declarations and their direct calls for the supported native targets. It leaves source definitions, unrelated externals, nonboolean parameters, unsupported targets and Runtime implementations unchanged. No `noundef`, purity, alias, unwind or calling-convention assumption was added.

The shared `finish()` applies this after the existing initial LLVM verifier succeeds, before O2 or native code generation. This preserves malformed-lowering diagnostic order; no assertion/crash was reproduced for malformed calls. O2 still has its final verifier. Application-bootstrap declarations are covered because they are present before `finish()`. There is no new FFI registry or public API.

### Legacy optimization metadata

`lower()` now uses its optimization argument for `DICompileUnit::isOptimized` and `DISubprogram::SPFlagOptimized`, independently of whether `source_debug` is present. JIT compilation explicitly supplies its IR optimization choice instead of leaving the lowerer argument false. Metadata emission policy, supported local variables and source locations are unchanged.

The label describes the selected IR optimization mode. JIT still intentionally uses `CodeGenOptLevel::Default`, even with IR O2 disabled; the accepted production AOT no-opt path still uses `None`. This correction does not promise new JIT source-debugging behavior or change cache/target policy.

### Optional emitters

No optional emitter machine policy was changed. The storage certifier records `machine_opt=Default` and its own IR recipe explicitly; that is a separate experiment, not current production no-opt equivalence. Its early return bypasses production `finish()`, including this Runtime annotation step. Its existing artifact claims must remain bounded to that recorded configuration; it is not newly qualified here. The private fixed-call research path does reach `finish()`, but still retains its separately documented machine default. No historical evidence was erased or relabeled.

## Verification method

The maintained ABI test constructs external Runtime calls, verifies declaration and call-site attributes, multiple bool arguments, bool return, idempotence, source-definition/unrelated-declaration exclusions, unchanged i8, and an unsupported-target control. Its emitted LLVM IR is compared with **actual pinned Clang O0/O2 output** for all three ABIs. Cross-target IR is not native Windows/macOS execution.

The maintained execution test runs ordinary VOID source with computed comparisons, stored/negated booleans, Runtime string-equality results and computed `print(flush=...)` arguments through O0/O2 JIT and independently linked AOT. It uses the existing abnormal-exit source preservation mechanism and verifies emitted DWARF.

For actual integration, a disposable copy of the real backend adds only LLVM-IR observation around ABI normalization and after `finish()`. It is compiled separately and linked to the existing Runtime/Core libraries; production contains no observation hook. The harness compiles and executes the source through real JIT and emits real AOT objects. Ten cases cover JIT, standalone AOT, profile none/line/full, each with O0/O2. Receipts distinguish lowerer's bare declarations, normalized declarations, final IR and metadata. Profile none must have no compile unit/subprogram metadata; other cases must reflect their actual O2 choice.

An initial metadata test incorrectly expected `DW_AT_APPLE_optimized` in Linux ELF. The failed receipt is retained as a **test-oracle error**, not a compiler regression. Verification uses actual LLVM metadata instead. An initial disposable fixture also used unsupported `!` syntax; its diagnostics are retained and the fixture uses VOID's `not`. The first incremental build regenerated CMake but its original Make invocation did not know the newly added test target; the separate target build succeeded. None of these receipts is counted as a successful first attempt.

## Focused qualification

The final source was rebuilt and the complete focused selection rerun after independent review moved normalization behind the initial verifier. No broad compiler matrix, sanitizer campaign or new performance gate is implied.

| Final check | Result |
| --- | --- |
| Release focused CTest selection | **9/9 passed**, 41.94 seconds: Runtime bool ABI/execution, native debug information, lexical scopes, Ninja initializer, Runtime coordination/generated code/callbacks and console source |
| Pinned Clang oracle | Linux x64, Windows x64 MSVC and Darwin ARM64; C++ O0/O2 declarations and calls agree on bool extension; this is cross-target IR evidence |
| Actual backend IR captures | **10/10 cases**, each checked immediately after normalization and after `finish()`; O0/O2 JIT, standalone AOT and none/line/full profile metadata |
| Installed compiler | O0/O2 JIT and independently linked AOT boolean workload passed; standalone DWARF verified |
| Actual installed extension | VS Code **1.144.0**, installed VOID extension **0.1.2**, pinned LLDB DAP; installed LSP readiness, F5 build, line-5 breakpoint, nested `dt=0.25` local and debug hover passed; host exit 0 |
| Preservation | Eight preexisting editor edits remain byte-identical; HEAD remains `c6015735`; task patch excludes them |

Both `voidc` and `voidls` build targets were processed and both components installed through the established CMake installation into `/home/shmellyorc/.local`. `voidls`'s installed bytes remain identical because this backend correction does not change its linked code; only installed `voidc` changed among the recorded previous installation files. The installed extension was not rebuilt or replaced. A task-owned editor profile/window exercised the actual installed extension and tools while Boss's normal Ninja window remained untouched. The harness's historical stdout says “normal user profile”; **this run used an isolated task profile**, as recorded by its exact argv.

Previous installed resources and tools are recoverable from `/home/shmellyorc/Documents/Void.Language-archives/llvm23-runtime-abi-2026-11-22/previous-installation.tar.gz`. Existing native approval records were neither replaced nor synthesized; ordinary compiler identity/approval checks remain in force.

The evidence bundle contains original diagnostics, the erroneous DWARF assertion receipt, command records, actual pre/post/final LLVM IR, C/C++ oracle outputs, focused XML/logs, installed-artifact hashes, editor receipt and task-only source patch. The disposable observer's own C++ compilation uses O0 to bound build cost; generated VOID O0/O2 selection and the shared production lowering code remain explicit in every capture. No performance claim is based on that observer.

Selected evidence: `/home/shmellyorc/Documents/Void.Language-archives/llvm23-runtime-abi-2026-11-22/evidence.zip`; the adjacent `evidence-manifest.json` records file hashes and verified ZIP integrity. Task-owned disposable binaries/objects and editor cache are removed only after that archive is verified. Source and maintained tests remain uncommitted for review.

## File inventory and preserved work

Production:

- `src/backend/llvm/backend.cpp`: shared annotation invocation, independent optimized metadata and JIT argument propagation.
- `src/backend/llvm/runtime_bool_abi.hpp`: bounded native boolean ABI helper.

Tests:

- `CMakeLists.txt`: one test executable and two focused CTest registrations.
- `tests/toolchain/RuntimeBoolAbi.cpp`.
- `tests/toolchain/runtime_bool_abi.py`.
- `tests/toolchain/runtime_bool_execution.py`.

Documentation: this report and `LEGACY_CHECKS_AND_OPTIMIZATION_AUDIT.md`. The protected root README, VOIDUP, GitHub Actions, LLVM pin and optimization policy are unchanged. Eight preexisting editor edits are excluded from this task's patch and checked against their starting hashes. Boss's Ninja/TestGame and native approvals were not edited. No staging, commits, branches, pushes or CI dispatch.

## Review disposition and limitations

Recommend the focused production corrections for Sacred Wrench source review. No legacy guard deletion, SLP enablement, loop-pass change or JIT machine-policy change is justified as part of this patch.

Native Windows/macOS qualification is not claimed. The Windows VM's verified OS is Windows 10 build 19045.6466 despite its label; the latest established guest-control receipt records a login refusal, so there is no verified usable channel available to this task. No credentials were guessed or VM settings changed. Boss has no native Mac. Cross-target Clang/LLVM checks remain useful ABI evidence, with native execution explicitly pending. Linux qualification is a Release compiler build with both generated O0/O2 modes; it is not a new Debug/strict-sanitizer acceptance campaign.
