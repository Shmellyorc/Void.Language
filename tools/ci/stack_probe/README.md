# Phase 23 native stack investigation

Investigation only. This does not implement arrays, enforce a compiler storage
limit or satisfy Phase 23 acceptance. The ordinary seven-job workflow is unchanged.
The new **Phase 23 native stack probe** workflow has only `workflow_dispatch`:
after the owner pushes it, select that workflow in Actions and run it manually.
Existing normal CI push/PR triggers still operate independently.

## What runs

On Ubuntu x64, Windows x64 and macOS ARM64, pinned LLVM/Clang 19.1.7 generates
four simultaneously live pointer-array homes, four root descriptors and a Frame.
Planned static sizes are 64/128/256 KiB, including descriptor/frame bytes, at O0/O2.
Each recursive call registers actual VOID runtime roots. Collection at the deepest
call sees every active frame. Elements are null nullable-reference payloads, not
uninitialized required-reference source values. Optimization stays enabled at the
selected level; escaped root storage keeps homes live, without volatile substitutes.

The runner configures ONLY this CMake subproject: six LLVM-independent runtime
sources and the harness. It never configures the repository root, links LLVM/ORC,
builds the frontend/backend/editor or runs CTest. Both harness/runtime and generated
objects use the selected O0/O2 mode, with matching Windows release CRT settings.

Every depth (1, 2, 4, 8, 16) runs in a separate child process. An unrelated fault
negative control runs once per size/mode. The parent times out hung children and
continues recording individual cases; unexplained failures fail the job.

## Explicit stack controls

- **Linux/macOS:** a dedicated pthread uses an explicitly mmap'd 1 MiB read/write
  stack with a 512 KiB inaccessible low guard and an inaccessible high page.
  OS thread APIs must report the requested usable extent and address. The large
  low guard is bigger than each measured native frame, preventing a subtraction
  from skipping the guard into unrelated storage. No stack-clash codegen option
  or optimization suppression is introduced. A separate 64 KiB signal stack can
  report faults after worker exhaustion. Only SIGSEGV/SIGBUS addresses inside the
  recorded low guard count as stack exhaustion. Core dumps are disabled.
- **Windows:** `_beginthreadex` uses `STACK_SIZE_PARAM_IS_A_RESERVATION` with a 1 MiB
  reservation. VirtualQuery walks the real allocation; GetCurrentThreadStackLimits
  is recorded separately. SetThreadStackGuarantee requests 64 KiB for reporting;
  usable headroom is therefore not identical to POSIX. Only the actual
  EXCEPTION_STACK_OVERFLOW code counts as exhaustion. Error dialogs are suppressed.
  A non-stack access violation is a failing negative-control outcome, never overflow.

These are worker-stack measurements, not main-thread RLIMIT_STACK experiments.
POSIX guard/alternate memory is outside the measured usable stack. Windows commits
pages on demand within its reservation, has its own guard/exception reserve, and
reports initial stack limits rather than an identical POSIX usable interval.
TLS, thread startup, runtime entry and machine call overhead also differ. The
machine/target and actual extent must be checked before calling generated code.
If a platform refuses these controls, the job fails with setup evidence; it does
not silently use a default stack or treat cross-compilation as execution.

Stack API references: [Windows reservation flag](https://learn.microsoft.com/en-us/cpp/c-runtime-library/reference/beginthread-beginthreadex),
[Windows stack limits](https://learn.microsoft.com/en-us/windows/win32/api/processthreadsapi/nf-processthreadsapi-getcurrentthreadstacklimits),
[POSIX explicit thread stacks and guard responsibility](https://man7.org/linux/man-pages/man3/pthread_attr_setstack.3.html).

## Results and failure policy

Each size/mode retains LLVM's native frame-size diagnostic and generated assembly.
The harness also measures frame-address stride across live recursive calls, checks
native Slot/Frame ABI, and verifies collection plus balanced cleanup on success.
POSIX success is emitted only after alternate-stack disable, disabled-state query
and unmapping all succeed. Each operation records its return value and immediate
native errno separately. A failed disable never proceeds to unmap registered memory.
The disable request retains the installed descriptor's valid size because Darwin's
[UNIX03 libc wrapper](https://github.com/apple-oss-distributions/Libc/blob/71bbe350ab79eef58113991d817ccc6165061a64/compat-43/sigaltstk.c)
checks that size even when SS_DISABLE is set.

Before the depth cases, each POSIX mode runs a separate `cleanup-check` child. It
observes the former zero-size disable request and records the exact result/errno.
Only that deliberate diagnostic request may report ENOMEM; any other error fails
the control. If it disables successfully, the child reinstalls the stack, then
requires the corrected disable/query/unmap lifecycle to succeed. Errors from the
actual cleanup are never ignored or retried. This lets the hosted macOS job verify
the diagnosis that run #1's combined cleanup error could not distinguish.

CMake cache path arguments use forward slashes and explicit PATH types. Native
Windows compiler/process paths and stack controls retain their original behavior.
The frame warning threshold of one byte requests evidence; it is not a compiler
limit. Caller return-address/alignment costs can make observed stride differ from
LLVM's reported frame size.

Cases with measured native frames consuming at most 75% of the stack must succeed.
Cases whose frame sum reaches/exceeds the entire stack must report verified
exhaustion. Intermediate cases are reported as boundary observations, not forced
to reproduce Linux thresholds. No arbitrary nonzero exit/signal is a pass.
Unexpected faults, setup/ABI mismatch, missing evidence, timeout, missing expected
overflow or failed cleanup fail the job. Expected exhaustion exits through the
harness with code 90 and platform evidence; an unrelated fault exits 91.

Artifacts (14 days) contain JSON results/environment/commands, a concise Markdown
summary, per-child logs, generated IR, assembly and small configure/build logs.
Binaries, the SDK and CMake object trees are excluded. Job summaries show host-native
outcomes only. Run #1 passed Ubuntu, stopped Windows at CMake path parsing, and
failed macOS normal-return cases during cleanup. Run #3 at commit ea86851 verifies all three complete native experiments and six
regressions per platform. See docs/ARRAYS_PROPOSAL.md Section 10 for the verified
frame/stride table and approval status. Local Linux results or cross-target objects
are not substitutes for these hosted executions.

## Local reproduction

Use a native supported host and LLVM 19.1.7, CMake and Ninja (or Unix Makefiles):

```sh
python3 tools/ci/stack_probe/run.py --llvm-bin /usr/lib/llvm-19/bin --output build/phase23-stack-probe
```

On Windows run from a configured x64 Visual Studio SDK environment and pass the
pinned SDK's bin directory. macOS requires ARM64 and the native Xcode SDK environment.
The runner chooses the host compiler target; it has no cross-target execution flag.
See `artifacts/summary.md` and `artifacts/results.json`. Every command and relevant
source/header hash is retained for reproduction. Do not infer a production numerical
storage policy or arbitrary-recursion guarantee from these bounded probes.
