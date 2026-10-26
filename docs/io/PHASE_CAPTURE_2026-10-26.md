# Bounded SIGSEGV phase capture — Golden Wrench handoff

26 October 2026. Baseline: `e24d3a95db03fd282c87a5d9b885471057e38798`.
Boss authorized phase capture with the unchanged Release runner, reconstructed
fixture and ASLR enabled. **The bounded investigation finished without SIGSEGV.
The original crash remains unexplained; no repair is established.**

## Bound and outcome

The declared limit was five attempts. Boss's supplied terminal transcript counts
as attempt 1; the agent ran exactly four further processes, attempts 2–5.
A 13-minute outer deadline plus a 30-second forced-stop limit bounded that
automated batch. It completed normally in 234.610 seconds, before the deadline.
No additional attempts were started after the limit.

| Attempt | Evidence source | Result | Elapsed |
|---|---|---|---:|
| 1 | Boss's older phase-script transcript, inferior 668878 | Both modes passed; normal exit. Entry markers only; no exact return/teardown markers. | O2 invoke entered at 59.814 s |
| 2 | Detailed agent capture | Both complete phase chains; both invocations returned 7; exit 0 | 59.895 s |
| 3 | Detailed agent capture | Both complete phase chains; both invocations returned 7; exit 0 | 60.472 s |
| 4 | Detailed agent capture | Both complete phase chains; both invocations returned 7; exit 0 | 58.708 s |
| 5 | Detailed agent capture | Both complete phase chains; both invocations returned 7; exit 0 | 55.532 s |

No SIGSEGV, other signal stop, nonzero exit or timeout occurred in the four
detailed captures. No failing PC/register set/backtrace/core exists from this
batch because no failure was observed. Passing repetitions do not establish a
repair or disprove any historical hypothesis.

Eight independent output checks passed: each detailed attempt produced matching
little/big-endian files for all 42 numeric vectors plus Pascal/raw text, 201 bytes
per file. The user-reported attempt's outputs were not independently inspected.
This is focused reproduction evidence, not a new full-suite acceptance run.

## Exact phase boundaries

The Release binary has ELF symbols and unwind information, but no DWARF line
tables. Entry markers alone missed returns and the inline NativeCode shared-pointer
release before `Jit::~Jit()`. Disassembly and independent subagent review established
these **normal-path** caller boundaries:

| Boundary | Offset from relocated `main` |
|---|---:|
| Async compile call | `+0x7c2` |
| Async compile returned | `+0x7c7` |
| Invocation call | `+0x642` |
| Invocation returned | `+0x647` |
| NativeCode shared-pointer teardown begins after progress output | `+0x699` |
| NativeCode shared-pointer teardown ends | `+0x43c` |
| JIT destructor call | `+0x43f` |
| JIT destructor returned; iteration teardown complete | `+0x444` |

Offsets apply **only** to runner SHA-256
`b42d5bcd782470be46a5a6009a56b8003fef5e8439e2b93caa04f0b027f3a3b6`.
The launcher checked runner/fixture hashes. At each new process's `main`, the
script recalculated the relocated base; it did not use static absolute PCs or
disable ASLR. Observed bases:

```text
0x55b085c6adf0
0x564d093f7df0
0x562e06a75df0
0x560ecf389df0
```

All eight markers occurred in order for each mode in all four detailed processes:
**64 ordered boundary events across eight complete pass chains**. Compile-call
`ecx` was 0 then 1; invocation-return `eax` was 7. Mode identification did not
depend on buffered stdout. The unoptimized source pass prints “Debug”; the host
executable remained the same Release build throughout.

Markers separate compilation, invocation and both teardown parts. They do not
decode individual native jobs, GC roots or exceptional cleanup landing pads.
Missing normal-path markers alone would not establish SIGSEGV; the actual signal,
exit and PC would decide.

## Capture controls and evidence

GDB used `set disable-randomization off`; SIGSEGV/SIGBUS/SIGABRT would stop and
print without passing the signal. On the first stopped live inferior, the script
would capture signal information, all-thread full backtraces (up to 64 frames
each), selected-thread registers, faulting/adjacent instructions, mappings/shared
libraries and a core, then stop the batch without continuing the inferior.
A timeout/debugger interruption would be classified from its recorded signal,
not inferred from a filename `crash.core`.

Boss explicitly authorized direct ptrace after supplying the manual result. The
automated run used approved debugger access outside the sandbox, against its own
child processes. Every attempt used a fresh private native working directory
under `/tmp/void-io-phase-capture-20261026-6vfao69v`; the source stayed at
`/tmp/VOID io async wire 1icf1laa/main.void`.

This source remains a **reconstructed candidate**, not preserved crash bytes:
SHA-256 `64b6a7abbf71eb627d80a0244acb9a0db061e5f6c6f5d582e38732d510befaab`.
The [research report](SIGSEGV_RESEARCH.md) retains provenance and original evidence
limitations. Runner/fixture hashes still matched after execution, as did all
53 source-freeze and seven protected hashes.

Permanent records, including diagnostic scripts exactly as run:

- [Manifest and attempt budget](evidence/phase-capture-2026-10-26/manifest.json)
- [Full debugger transcript](evidence/phase-capture-2026-10-26/session.log)
- [Boundary events](evidence/phase-capture-2026-10-26/events.jsonl)
- [Process results](evidence/phase-capture-2026-10-26/results.json)
- [Validated timings, returns and wire oracles](evidence/phase-capture-2026-10-26/summary.json)
- [GDB script](evidence/phase-capture-2026-10-26/capture.gdb)
- [Checksum/deadline launcher](evidence/phase-capture-2026-10-26/run-capture.sh)
- [Static main disassembly](evidence/phase-capture-2026-10-26/main-disassembly.txt)
- [Evidence hashes](evidence/phase-capture-2026-10-26/sha256.json)

Scripts preserve original private paths and a one-run guard. They record this
bounded experiment; they are not a production framework or authorization to
extend the attempt count.

## Interpretation and stop point

Normal JIT compilation took 20.741–22.975 s; O2 compilation took 34.415–38.010 s.
Invocations took 0.071–0.088 s. These are phase observations, not evidence that the
historical crash occurred during compilation. Elapsed time and stdout alone still
cannot locate it.

No failure phase was identified. **No compiler-only, GC-budget, worker-count,
forced-scheduling or per-job isolation experiment was run.** Native completion/
cleanup state would require examination if a captured fault implicated execution;
this passing batch does not claim decoded Task/Work state or individual worker
completion histories. That conditional obligation was not triggered by a failure.

The limit is exhausted and execution has stopped. No production edits, compiler/
Runtime rebuilds, hosted CI, commits or pushes occurred. Return this non-reproduction and
its preserved records to Golden Wrench. The original SIGSEGV remains an open
reliability finding; any further run needs a new bounded scope from Boss.
