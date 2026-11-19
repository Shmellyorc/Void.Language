# G75 — Debug compiler correctness repair

18 November 2026. Boss explicitly authorized the minimum correction to existing
allocation-elision use bookkeeping, its tests and documentation. G75 feature work
has not resumed. The accepted sentinel repair is preserved; the authorized
frontend/onboarding follow-up below passes its complete Linux regression gates. G69 remains on its existing
hold; G76 has not begun. Existing G75 presentation/editor work is preserved.
No staging, commits, pushes, installation into user prefixes or publication was
performed. Existing disposable installation/relocation fixtures ran as part of
the mandatory regression suites; installed tools and user settings were retained.

## Current frontend/onboarding follow-up

Boss subsequently authorized resolving the two frontend depth failures and the
onboarding mismatch before any G75 feature continuation. The final integrated changes pass the complete Linux regression gates; the
sentinel closeout below is retained as historical evidence. No earlier failed result is relabeled as a pass.

### Direct evidence and bounded corrections

GDB ran the real compiler through an exec wrapper setting **both soft and hard
RLIMIT_STACK to 1,048,576 bytes**, with ASLR enabled. No debugger security setting,
Debug assertion, sanitizer, source nesting limit or test timeout was weakened.
The pre-change binaries, source, original archives and 54-path inventory were
preserved before editing; the accepted sentinel backend remains byte-unchanged.

| Path | Direct failure evidence | Minimal correction |
| --- | --- | --- |
| 400-call receiver rejection | The recursive semantic path consumed 3,664 bytes per level across `expression`, `expression_impl` and `method_expression`. GDB stops in exception unwinding after an exhausted nested expression produces the existing error type and member recovery throws `Poison`. The stack is already at its limit. | Move only fixed-array-default temporaries into the existing style of non-inlined case handler. The dispatcher changes from 1,048 to 72 static frame bytes; the same 256-level semantic guard and recovery remain. |
| 80-level defer rejection | GDB shows 61 parser statement frames consuming 15,904 bytes each including return addresses, plus a 54,352-byte outer parser frame. Stack exhaustion occurs before the existing 64-level guard. | A thin statement dispatcher routes defer to its own handler before ordinary statement temporaries are reserved. The two relevant static frames total 5,904 bytes instead of 15,896. The original defer parser body and 64-level guard are unchanged. |
| Valid deep defer acceptance | After parser rejection was repaired, a legal 60-level body reached semantic analysis and overflowed through nested `lambda_expression`/`analyze_one`/`sequence`/`statement` calls. Each old semantic statement frame consumed 22,176 bytes including its return address. | Handle the existing deferred statement before entering the ordinary statement frame. The deferred dispatch frame is 664 static bytes instead of 22,168; all checks, cleanup creation and the original `s.flow` assignment retain their order. |
| Valid deep receiver execution | An additional real 1 MiB JIT probe also fails on the frozen pre-change Release compiler. GDB identifies recursive frontend `Lowering::expression` ordinary-call frames; the combined builtin/ordinary-call case reserves 33,704 static bytes per level. | Finish builtin dispatch in a separate existing `frontend_case` boundary before recursing into an ordinary receiver. The ordinary-call frame drops to 1,336 static bytes. All builtin conditions and lowering bodies retain their original order and behavior; no LLVM pass changed. |

Static byte figures come from Clang 23.1.4 `-fstack-usage` compilations using the
actual Debug recipe; GDB stack-pointer differences independently confirm the
pre-change parser and semantic frames. Return addresses account for the stated
eight-byte differences. The extra frame measurements are diagnostic builds only;
normal configured build flags remain intact.

The receiver-only intermediate correction passed 100 rejection repetitions while
all three defer controls still crashed. The parser correction then let the valid
defer semantic failure become reachable. Each intermediate source/patch and
unfavorable result is retained independently. These are case-boundary changes in
one private frontend implementation, not a new recursion algorithm, new stack,
heap-backed worklist, language limit or dependency.

### Upstream and GitHub applicability

The existing [Clang D66361](https://reviews.llvm.org/D66361) was evaluated first.
Its new-stack approach does not satisfy the unchanged VOID 1 MiB contract and was
not adopted. Boss's subsequent request prompted explicit public web and GitHub
issue, PR and commit searches, with direct API verification of state/history.

- [Clang #88546](https://github.com/llvm/llvm-project/pull/88546) diagnosed large
  recursive initialization objects, but was subsequently
  [reverted for leaks/performance](https://github.com/llvm/llvm-project/commit/d0f718e06848774a4e9d0b253cf75c1408b5f41a).
  This is historical evidence for frame pressure, not a current fix to import.
  The pinned header still has a by-value failed candidate set. No heap-object
  conversion from that reverted patch is part of VOID's repair.
- [Clang #208106](https://github.com/llvm/llvm-project/pull/208106) merged a Debug
  visitor-inlining correction. Its `ALWAYS_INLINE_UNLESS_DEBUG` implementation
  is present in the pinned SDK's `Compiler.h` and `StmtVisitor.h`; it addresses
  Clang's visitor, not VOID's private traversals. It supports measuring and
  preserving explicit case boundaries in Debug as well as optimized builds.
- [Clang #145734](https://github.com/llvm/llvm-project/pull/145734) merged reuse
  of an existing semantic worklist. A worklist rewrite is unnecessary for VOID's
  bounded traversals once their measured per-level frames fit the existing limits.
- [Clang #224393](https://github.com/llvm/llvm-project/pull/224393) and
  [#175816](https://github.com/llvm/llvm-project/pull/175816) remain unmerged at
  query time. Their depth/temporary-lifetime proposals were not represented as
  available fixes or adopted. [MLIR #239111](https://github.com/llvm/llvm-project/issues/239111)
  reports a related parser-overflow pattern, not a solution for VOID or its pin.

`upstream-research-summary.json` records source URLs, verified states, the
reversion, pinned-header checks and adoption decisions. No toolchain upgrade,
optimization setting or exception-lifetime policy change was made.

### Contracts and repeated probes

The final integrated Debug, Release and strict compilers each pass **100 receiver
and 100 defer rejection repetitions** at the original real 1 MiB limit: **600/600**.
Every status, stdout and diagnostic byte matches the frozen pre-change Release
contract for the same source paths. Expected status remains 1, source positions
and `VOID1107` nesting diagnostics remain, and later `VOID2003` recovery remains
where the existing fixture requires it. Intermittent baseline SIGSEGVs remain
preserved; no successful repeat is substituted for an earlier failed attempt.

The strengthened existing `semantic_depth.py` retains every original negative
assertion and limit. It adds valid defer checks at 16, 32 and 63 levels, and
executes the legal 124-call receiver through uncached O0/O2 JIT under the same
1 MiB child limit. Both existing registrations consume this maintained script.
The 63/64 defer boundary and existing receiver acceptance/rejection boundaries
remain. Separate explicit 1 MiB probes cover the 124-call receiver and 16/63-level
defers through O0/O2 JIT, build and native execution: **18/18 per profile**.

### Onboarding correction

`docs/FIRST_PROGRAM.md` contained an extra debugger example outside the canonical
example list, creating ten `void` fences where the unchanged test expects nine.
That program is valid VOID; the defect was documentation/example drift, not a
language bug. The guide now keeps its canonical hello source for debugging,
uses line 4 for the print breakpoint, and describes `Hello, world!` and exit 0.
The onboarding assertion, canonical source fixtures and compiler semantics were
not changed to accommodate the documentation.

### Integrated qualification and impact

| Profile | Passed | Failures | Skips | Full CTest wall seconds |
| --- | ---: | ---: | ---: | ---: |
| Debug | 516/516 | 0 | 0 | 3599.95 |
| Release | 503/503 | 0 | 0 | 1157.43 |
| Strict | 493/493 | 0 | 0 | 3475.97 |

These are fresh **complete** runs after the final source audit, with matching
final compiler/source/test hashes and the original limits/failure policies.
No earlier completed subset is substituted for a full final matrix.
`complete-suite-results.json` retains every registered result and the required
case outputs. Both frontend registrations, onboarding, allocation-elision and
escape-analysis O0/O2 paths, and CLI/VOIDUP contract/presentation tests pass in
all three profiles. Actual emitted timing JSON/schema checks remain included.

| Original sentinel-affected case | Required behavior retained | Debug / Release / strict |
| --- | --- | --- |
| Managed native | Deliberate `void_d4_link_value` undefined-symbol link failure, exit 1, no published artifact | Pass / Pass / Pass |
| Legacy manifest/minimal | Normal application exit 19, empty stdout/stderr | Pass / Pass / Pass |
| Mixed native | Interoperability succeeds, exit 0 and expected resources/records/spans output | Pass / Pass / Pass |

The exact original minimal is additionally re-executed through uncached JIT and
linked AOT at O0/O2 on each final compiler; exit 19 remains. The new terminator
fixture retains its `1018` output and exit 0. Earlier G74 evidence still proves
only the legacy/minimal original sentinel case independently reproduced and
backtraced on unchanged G74; no broader baseline claim is added.


All fourteen accepted fixture inputs (the eleven allocation-elision fixtures,
original minimal, valid receiver and valid defer) emit O0/O2 objects. **28/28
Debug and 28/28 Release objects are byte-identical to the frozen pre-follow-up
Release outputs**, with matching input hashes and options. LLVM verification
continues before optimization and after O2. This is bounded artifact evidence,
not a proof for every possible source or sanitizer metadata configuration.

Bounded Release compiler A/B measurements compare the preserved pre-follow-up
Release binary with the final repaired Release binary, using 20 alternating
paired samples per case/mode and two excluded warmups. All raw samples, outputs,
arguments, hashes, medians and p95/worst values are retained. Detailed timing is
text; status is explicitly off. Task-owned builds/tests/capture/archive jobs
were quiescent during sampling; ordinary desktop/VM activity remained.

| Case | Detailed timing | Before median ms | After median ms | Delta |
| --- | --- | ---: | ---: | ---: |
| tiny-check | Disabled | 5.237 | 5.152 | -1.61% |
| tiny-check | Text enabled | 5.227 | 5.170 | -1.09% |
| channel-check | Disabled | 42.683 | 42.654 | -0.07% |
| channel-check | Text enabled | 43.408 | 42.923 | -1.12% |
| deep-receiver-emit | Disabled | 33.529 | 31.831 | -5.06% |
| deep-receiver-emit | Text enabled | 33.484 | 31.401 | -6.22% |

No substantial performance regression was observed in these paired samples. Median wall times were 0.07% to 6.22% lower; small p95/worst fluctuations and all unfavorable samples remain in the raw evidence. This is bounded frontend/AOT evidence, not an
all-workload or cross-platform performance guarantee. The original G75 benchmark
archive remains unchanged. Boss's earlier JSON-report cost acceptance is a
separate, still-pending decision; this repair does not grant that acceptance.

One intermediate Debug AOT-generation build command terminated with SIGSEGV;
its make output and kernel general-protection-fault event are retained. The exact
frozen runner passed ten direct replays and twenty additional debugger replays,
all producing the same object bytes. No core was available from the original
attempt, and no production change is claimed to repair its unestablished cause.
The subsequent Boss-directed investigation checked the exact host journal event
(PID 3205067, 2026-11-18 23:55:14.527941 UTC), the surrounding 18-minute system/app
timeline, crash-report locations and the structured coredump journal. No saved
original core, OOM kill, GPU reset, storage error or reported hardware error was
found in that window. `coredumpctl`, Valgrind and historical sar/atop/process
accounting data are unavailable; no package was installed.

ELF-aware symbolization places the preserved image's projected fault at
`MachineBasicBlock` dominator-tree `CalculateFromScratch`. A **normal-control**
core stopped at that exact instruction confirms the path through
`MachineDominatorTreeWrapperPass`, `write_object`, `emit_object` and
`emit_async_object`. This establishes the reachable emission phase, not the
original corrupt state or its cause. The control continued successfully. It is
never represented as a failing compiler core.

A task-local versioned Python/GDB collector now saves a kernel or debugger core,
registers, signal information, executable bytes, all-thread backtraces, mapped
libraries, input/binary hashes, arguments and limits. Child-local core limits and
`coredump_filter=0x3f` preserve executable mappings without changing global
settings or stack limits. Deliberate null/GP-fault calibration programs verified
core creation and reopening; the kernel PC/file-offset translation and actual
captured code bytes were checked directly. The wrapper preserves a failing
signal classification instead of accepting GDB's own zero status as success.
An initial intentionally stopped control exceeded the diagnostic harness's
90-second deadline while expanding full local objects; its log is retained.
Core generation was moved before formatting, with bounded logs and ordinary
all-thread backtraces. No compiler/test timeout was changed.

The original thirty clean replays plus **72 additional replays** remain clean:
16 serial normal, 32 concurrent normal/O2, eight diagnostic heap-layout controls,
and 16 debugger runs in the original build directory. One additional versioned
wrapper AOT smoke also succeeded. These are **102 recorded replay-series runs
with zero recurrences**, not a causal diagnosis or a claim that a production
patch fixed this event.

The broader timeline records Steam's ignored 32-bit preload warning at
23:55:11.501 UTC, an added game-associated process at 23:55:13.243, and the AOT
fault at 23:55:14.528. Boss confirms playing RimWorld. No such loader warning
appears in the original compiler build log; readable current Codex parent
loader environments and the normal control's library maps show no Steam
preload/overlay. The Steam scope's later memory/swap peaks cover its entire
roughly two-hour lifetime, not an instantaneous crash-time sample. The timing
is context and does not establish game, loader or hardware causation.

**Boss explicitly directed recording this as a non-blocking, non-reproducing
event and continuing qualification.** Expanded investigation has stopped; the
original crash, all controls, calibration cores, normal compiler core, helper
sources and unfavorable harness evidence remain retained in `aot-capture`.
No LLVM/backend/ABI change was made for this event.

The first Debug/Release complete-matrix attempts were intentionally interrupted
once the additional lowering failure was directly established. Their logs,
process identities, interruption status and source stages remain retained.
They are not counted as final acceptance. A final source audit additionally caught
the moved defer handler returning its flow without retaining the original
`s.flow` assignment. The assignment was restored before acceptance. The
pre-audit Release 503/503 and interrupted Debug/strict matrices, binaries and
receipts remain in `before-flow-assignment`; none is used as the final matrix.
Fresh complete matrices use the final integrated compiler/test identities and
original limits/failure policies.
Same-filesystem scratch permits the hard-link test; `VOID_IO_SECOND_VOLUME=/tmp`
provides the actual distinct filesystem for cross-volume checks. Retained
native diagnostic roots are explicitly created mode 0700.

### Qualification limits

Native qualification is **Linux x64 only**, using the existing pinned LLVM
23.1.4 SDK. Windows and macOS did not run native tests and remain unqualified.
The strict configuration is the existing RelWithDebInfo ASan/UBSan build with
fatal runtime settings and leak detection enabled. The SDK itself was not
rebuilt with sanitizers, and generated JIT code is not thereby proven fully
instrumented. RTTI/vptr qualification remains unavailable with this SDK.

The existing registrations remain Debug 516, Release 503 and strict 493. The
strict matrix already excludes ten other native/LSP stack wrappers; no exclusion
was added by this follow-up. Both required frontend registrations and the
explicit final receiver/defer repetitions retain the real 1 MiB child limit in
all three profiles. `registered-case-differences.json` records the exact lists.

The corrected canonical first-program workflow was exercised with the final
Release compiler, full Debug symbols and actual Linux LLDB at line 4. The editor
unit suite passes 10/10 with the freshly rebuilt `voidls`. An initial unit rerun
supplied the wrong server environment-variable name and failed setup; that log
is retained separately, and unchanged tests passed with `VOIDLS_TEST_SERVER`.
The earlier real VS Code slow-task/exception override evidence is preserved;
those interactive UI cases were not replayed during this frontend follow-up.
Independent Golden Wrench review, native Windows/macOS acceptance and Boss's
previously pending G75 JSON-cost decision are not claimed here.

This authorized follow-up touches exactly **five paths**:

| Path | Change |
| --- | --- |
| `src/frontend/compiler.cpp` | Four measured case-boundary corrections in parser, semantic checking and frontend lowering |
| `tests/integration/semantic_depth.py` | Additional valid-defer acceptance and low-stack legal-receiver JIT coverage; original negative assertions and limits retained |
| `docs/FIRST_PROGRAM.md` | Canonical hello/debugger documentation correction |
| `docs/projects/G75_DEBUG_CORRECTNESS_REPAIR.md` | Current engineering/evidence record; historical sentinel evidence retained |
| `docs/projects/G75_CLI_EXPERIENCE_IMPLEMENTATION.md` | Current status and complete working-file inventory |

The working tree now contains **56 paths: 41 modified and 15 new**, versus 54
at the start of this follow-up. The two additional paths are the tracked guide
and semantic-depth test above. The complete exact path list is in the
[implementation inventory](G75_CLI_EXPERIENCE_IMPLEMENTATION.md#exact-changed-file-inventory)
and `changed-file-inventory.json`. The accepted sentinel backend and all other
G75 implementation files remain preserved.

Current raw evidence remains at `/tmp/void-g75-frontend-repair`. The follow-up
archive is `/home/shmellyorc/Documents/Void.Language-archives/g75-frontend-regressions-2026-11-18.zip`,
SHA-256 `b13a755d4c83539d24aeae745f7fc838836001752b8f215ec71991743c7728b9`: 848 selected files,
55,354,357 original bytes, 5,193,338 ZIP bytes.
CRC, every member hash and actual extracted-restoration hashes were verified.
Maintained engineering reports remain outside the ZIP to avoid a self-referential
archive hash. Both original G75 archives remain byte-unchanged and were reverified.

The private calibration/normal-control core bundle is `/home/shmellyorc/Documents/Void.Language-archives/g75-aot-capture-controls-2026-11-18.zip`,
SHA-256 `936a46194d0ce6998313efc54c6a8352188c38ec73748ac5e1d1e3d165a61ec0`, mode 0600, 90,203,039 ZIP bytes.
Its 22 selected files include the exact frozen runner, calibration
executables/cores, normal compiler control core, versioned capture tools and
private surrounding journal window. Every extracted hash was verified. It
contains **no original failing core**; none exists in the available records.
The human-readable follow-up archive includes the evidence summaries and core
bundle receipt; the full private journal/core memory stays in this private bundle.

Final checks: `git diff --check` passes without output; staged diff is empty;
HEAD and locally recorded `origin/main` are unchanged. The exact 56-path inventory
and all edited relative document targets pass verification. All 51 protected archived G75 implementation/fixture paths, including the
accepted backend, remain preserved. No
cleanup of original sources, unique failures, frozen binaries, recovery copies,
installed tools/settings or unrelated files was performed.

The regression gate is clear on the qualified Linux host. G75 feature work has
not resumed; this result is submitted for the requested integrated review.


---

The following sections retain the earlier sentinel-only closeout and its exact
historical outcomes and archive. The authorized frontend follow-up supersedes
its earlier request for Boss scope direction; it introduces no new milestone hold.

## Evidence preserved before editing

The original G75 qualification ZIP was verified before production edits: CRC and
every member SHA-256 matched its manifest. Archive:
`/home/shmellyorc/Documents/Void.Language-archives/g75-qualification-2026-11-18.zip`,
SHA-256 `afb1c7c2f24d5a3fd4d930f8c9ab481dce0a804b18b9a84068c105e20144e116`.
It retains all three original unfavorable test logs, the exact legacy/minimal
source, G74 and candidate Debug backtraces, and the earlier qualification inputs.

New task evidence is under `/tmp/void-g75-debug-repair`. Before changing the
backend, the task saved pre-repair Debug/Release/strict compiler binaries and
hashes, the pre-repair backend, HEAD's G74 backend, the existing implementation
report, working diff and full source-file status inventory. Required active
SDKs/providers, installed tools, user settings and recovery material were retained.

The baseline distinction remains: only the legacy/minimal case was independently
replayed and backtraced on untouched G74. Managed-native and mixed-native logged
the same assertion class but did not receive their own complete G74 baseline
reproduction. Their successful repaired executions do not retroactively establish
those missing baseline backtraces.

## Exact root cause and minimum correction

`block_allocation_elision` indexes managed-reference uses. An instruction use
records its real block and instruction index. Return/branch terminator uses are
recorded as `Use::escape` with index `~0u`; they do not designate an instruction.
Valid typed-IR branch conditions are boolean, so the managed-reference filter
normally excludes those branch operands. Managed returns do enter the index.

The heap-affinity propagation loop bound an instruction reference before checking
`use.kind`. For an unproven managed value returned from a function, this indexed
`instructions[4294967295]`. The confirmed Debug backtrace's vector had 29 entries.
Debug's libstdc++ bounds assertion aborts with SIGABRT. Optimized Release or
RelWithDebInfo sanitizer passes did not establish that this access was valid.

The production correction is one classification guard, with two explanatory
comment lines, before that instruction access:

```cpp
if (use.kind != Use::field && use.kind != Use::store) continue;
```

Those are precisely the two existing cases that propagate heap affinity.
Call/alias/escape records did not update this proof before the repair; they now
avoid a needless instruction lookup. Field/store behavior is unchanged. The
separate escape analysis still rejects escaping allocation candidates. No
assertion suppression, signal handler, optimization bypass, LLVM pass change,
allocation-policy redesign, language-semantic change or native ABI/authorization
change is part of this repair.

## Use-index consumer audit

| Consumer | Index invariant / outcome |
| --- | --- |
| Use construction | All field/store/call/alias instruction records receive the enumerated in-range instruction ID. Only terminator escape records receive `~0u`. The managed operand itself is range/type checked before a record is added. |
| Heap-affinity propagation | Repaired guard admits field/store before all instruction, type and unproven-array accesses. Other use kinds have no propagation effect here. |
| Allocation candidate walk | Escape is rejected before field/store indexing or alias/call propagation. Its earlier lifetime bookkeeping may see the sentinel, but `safe=false` discards that candidate before any retirement or indexed use of the recorded lifetime. |
| Alias/call/field/store consumers | Reachable only after escape rejection and their own kind checks. Their positions come from instruction enumeration. |
| SSA copy retirement | Computes last use, then explicitly rejects `last == ~0u`; returned references keep their root home through frame exit. No instruction is indexed by that sentinel. |
| Local-use/event consumers | Separate positions built only from actual instructions, including receiver places; no terminator sentinel enters them. |

No adjacent consumer required a further production change.

## Bounded upstream check

Online research preceded expansion of the audit. LLVM's established use/owner
interfaces and classification before using instruction-specific data support the
same invariant. The [LLVM Programmer's Manual](https://llvm.org/docs/ProgrammersManual.html#iterating-over-def-use-use-def-chains)
illustrates checking a use's user kind before instruction operations.
[TargetInstrInfo](https://llvm.org/doxygen/classllvm_1_1TargetInstrInfo.html)
defines a special `CommuteAnyOperandIndex` whose semantics must be resolved by
its operand-selection interface. The pinned 23.1.4 headers independently confirm
that convention; current online doxygen describes a newer development version.
[GCC's SSA operand documentation](https://gcc.gnu.org/onlinedocs/gcc-15.2.0/gccint/SSA-Operands.html)
likewise restricts PHI-specific index operations to PHI uses.

These are existing APIs/invariants, not a patch fixing VOID's private record.
No directly applicable upstream bug-fix commit was identified or adopted. The
repair uses VOID's existing kind tag; it imports no dependency, container or pass.
No proposed or abandoned upstream patch is being treated as a merged solution.

## Focused regression

The new `terminator_escape.void` fixture returns incoming managed references
straight through a function and through both arms of a branch. Entry constructs
three objects and returns 1018 for input 1000. The objects escape through calls
and must retain all three constructions.

The unchanged pre-repair Debug compiler was run against this exact fixture in
both O0 and O2 modes. Both abort with the original bounds assertion and status
-6. Raw stdout/stderr, argv, source and result receipts were retained before the
production edit.

The fixture is part of the existing allocation-elimination case registry and
CMake AOT fixture list. The original shared harness checks ordinary results,
exact construction counts, precise root/publication validity, forced collection,
cold allocation-failure classification, reclamation and recovery. It runs JIT
O0/O2 and separately linked AOT O0/O2. All existing count/failure oracles remain
unchanged. This tests escape safety as well as the absence of an abort.

## Stale fixture assumptions found by the wider run

The first full runs retained additional failures in console, logging, managed
initialization, reference/value composition, interfaces, Math, memory/binary
ownership and channel lease fixtures. The console/logging cases and the GC-family source cases were compared against
an isolated pre-repair backend,
compiled from the preserved pre-edit source with the same Release recipe and
linked with the same unchanged companion objects and libraries. This comparison
is a G75 pre-repair control, not a claim of independent G74 reproduction.
Family harnesses can stop at their first failed oracle; those controls do not
establish a separate pre-repair failure/backtrace for every later GC fixture.
The related marker corrections are identified by their explicit heap-activity
preconditions and are verified against the original final-result/GC assertions.

The console fixture expected an allocation callback to close Runtime before
first I/O, but its private scalar-only gate object was eligible for the existing
allocation-elision proof. The callback never ran, so output correctly occurred
and the fixture's shutdown assertion failed. Both backends failed identically.
The corrected fixture returns the gate through a helper, making its escape and
construction observable. Its existing closed-console diagnostic, metadata refusal,
root and lifecycle assertions remain. This also exercises a managed-return
terminator under the repaired compiler, without disabling optimization.

The logging full-sequence fault sweep also fails on the pre-repair backend at
the same `io._error` interpolation diagnostic. Isolated allocation-only sweeps
pass with both backends; earlier operations change which allocation budget reaches
error-message construction. The runtime already reports string-join allocation
failure through this diagnostic, but the test's accepted fault-family list omitted
it. The test now recognizes the exact interpolation diagnostic only with the
`/@stdlib/io.void::_error` source context, only inside deliberate allocation
sweeps. Other unexpected runtime errors still propagate. Existing construction,
precise-root, descriptor, filesystem-identity, partial-write, cancellation,
publication, reclamation and recovery assertions are unchanged. No production
diagnostic or error handling was changed.

The managed-initialization diagnostic probe measured three constructions instead
of the expected 303, with valid roots and commits, empty frames and only two
collection epochs. Its 300 discarded temporary objects had been elided. The
interface/reference/composition stress loops and Math/memory/binary marker objects
have the same discarded-allocation assumption. Marker values now cross an
explicit managed-return boundary under the existing escape proof; initialization
also performs a fresh-versus-live identity comparison. The original numerical
results, exact construction counts, fault expectations, root checks and recovery
oracles remain. These managed-return helpers also exercise the repaired sentinel
path in real ownership/GC workloads. Future changes to the interprocedural elision
proof would require reviewing these compiler-specific probe preconditions.

The channel lease fixture waited for a construction hook on a discarded callback
frame. The original full Release run timed out at its unchanged 600-second limit.
An original-source pre-repair backend control also timed out; with the corrected
escaping callback marker the pre-repair Release backend completes all 38,002
checks. This distinguishes the fixture's missing rendezvous from a repair-induced
Runtime regression. No timeout or synchronization oracle was relaxed.

The native G4 hard-link negative test initially could not create a link from the
XFS build tree into tmpfs `/tmp` (EXDEV). A task-owned same-filesystem scratch
location resolves that setup error without changing the security assertions.
That same-filesystem default makes the explicit cross-volume test report its
normal skip code 77. Its Debug/strict confirmations set `VOID_IO_SECOND_VOLUME`
to tmpfs `/tmp`, while the primary remains in the XFS build. Both actual devices
are exercised; the initial skip records remain.
The resumed Debug and strict G06 native tests correctly refused the retained diagnostic
root created with mode 0775 (`private fixture admission failed: 2/0`). The
existing [G08 qualification record](../compiler-health/G08_COMPILER_PERFORMANCE.md)
describes the same setup requirement. Task-owned diagnostic roots were changed
to 0700; original modes, directory identities and both failed runs were retained.
The G06 tests are confirmed again under private roots, with no admission-policy
change.

The unchanged onboarding document example check fails before invoking any
compiler; its status is reported separately below, with no unrelated tutorial
rewrite in this repair.

Original failures, before/after console results, the original logging test,
pre-repair full-sequence logging failure and retained diagnostics remain in task
evidence. These are test setup/recognition corrections; no second production
repair or optimizer redesign was introduced.

## Verification results

The exact original minimal reproducer now exits **19**, with empty stdout/stderr,
under Debug, Release and strict, in uncached JIT O0/O2 and separately linked
AOT O0/O2. The eighteen recorded minimal/new-fixture invocations meet their
expected status and output. Both original Debug negative controls for the new
fixture aborted before repair; the repaired fixture retains exactly three
constructions and returns 1018. Existing allocation-elimination and escape/
foreign-heap checks retain their original assertions.

| Original affected case | Expected application/compiler outcome | Repaired Debug | Repaired Release | Repaired fatal strict |
| --- | --- | --- | --- | --- |
| Managed native | Deliberate final-link rejection, `undefined symbol: void_d4_link_value`, exit 1; no rejected artifact/native payload published | Expected 1 and diagnostic asserted | Expected 1 and diagnostic asserted | Expected 1 and diagnostic asserted |
| Legacy manifest / missing context | Normal application exit 19; empty stdout/stderr | 19 | 19 | 19 |
| Mixed native | Native interoperability message, normal exit 0; JIT, independent build and relocation | 0, expected output | 0, expected output | 0, expected output |

These are internal expected application/compiler statuses; each containing test
harness exits 0 on its expected result. The deliberate linker rejection remains
a rejection. Native approval, deterministic lock, pre-load refusal, invalid
primitive transport, package/artifact and forwarded-application checks remain.

Final outcomes below count **unique registered test names across the complete
sweeps, preserved completed passes and explicit final-input/setup replays**.
They are not described as single uninterrupted all-green runs. Every original
registration has an authoritative result; **zero final skips** remain.

| Profile | Final passed / registered | Remaining failures |
| --- | --- | --- |
| Debug | **513/516** | `text_encoding_semantic_depth`, `source_cli` (independent frontend 1 MiB SIGSEGV); `onboarding_examples` (unchanged static document mismatch) |
| Release | **502/503** | `onboarding_examples` only |
| Strict RelWithDebInfo | **492/493** | `onboarding_examples` only; applicable compiler/runtime/native cases pass with fatal ASan/UBSan and leak detection |

The first full Release attempt was **487/503**, with sixteen failures. Its fifteen
fixture/setup failures passed the registered replay, followed by **12/12** final
fixture-input confirmations. The documentation failure remains. Debug retains
23 completed passes plus its 493-name remaining sweep; strict retains 14 plus
its 479-name remaining sweep. Their original failed attempts and one cross-volume
skip per resumed sweep remain visible. Debug's private-root/final-input replay
passes **4/4**, followed by **1/1** actual cross-volume; strict's private-root and
actual cross-volume confirmation passes **3/3**. No timeout, diagnostic, ownership,
root, fault or security oracle was relaxed to obtain these results.

Focused ordinary and independently linked allocation-elimination O0/O2 checks
pass across all three profiles, as do escape analysis and its foreign-heap
negative controls. The new fixture records three constructions in both JIT
modes, with 474 checked roots, eight forced polls and an allocation-failure path
that remains observable. Existing console tests pass 4,150 checks and logging
passes 1,690 checks in each profile. The corrected Release channel lease control
and the pre-repair-backend fixture control each pass 38,002 checks.

`final-suite-results.json` enumerates every name, final result, originating
log/JUnit and original failed/skipped attempts. Expected linker/runtime/native
refusals remain expected failures at their original classifications. The
independent Debug frontend failures are detailed immediately below.

## Independent Debug frontend blocker

The complete repaired Debug sweep also fails `text_encoding_semantic_depth`
and `source_cli`: `voidc check` terminates with SIGSEGV under the existing
**1,048,576-byte** child stack limit, instead of returning nesting diagnostic
status 1. This occurs before LLVM lowering; the allocation-elision repair is not
executed. Original failed output, the receiver input captured at child launch,
and the source-cli defer failure remain retained. The defer input is recreated
byte-for-byte by the unchanged test generator; that original child had no crash
receipt hook, so its original temporary source was not independently retained.

The two exact input forms were tested three times each against the repaired
Debug compiler, the frozen pre-repair G75 Debug compiler, and the retained fresh
unchanged G74 Debug compiler:

| Compiler | 400-call receiver, status in three 1 MiB runs | 80-level defer, status in three 1 MiB runs |
| --- | --- | --- |
| Pre-repair G75 Debug | 1, 1, -11 | -11, -11, -11 |
| Repaired Debug | -11, -11, -11 | -11, -11, -11 |
| Unchanged G74 Debug | 1, -11, -11 | -11, -11, -11 |

A separate diagnostic control at 8 MiB returns the expected diagnostic/status 1
for both inputs with all three compilers. **That control is not acceptance**;
the real 1 MiB assertions and limits are unchanged. Receiver variability is
recorded, not averaged into a pass. These comparisons establish a pre-existing
frontend low-stack failure; no new frontend function-level backtrace or complete
root-cause proof is claimed.

Before extending this investigation, a bounded primary-source upstream check
found Clang's [merged D66361](https://reviews.llvm.org/D66361) and current
[stack-exhaustion implementation](https://github.com/llvm/llvm-project/blob/main/clang/lib/Basic/Stack.cpp).
Clang can diagnose low remaining stack and move selected recursive operations to
a new stack. That is an established general treatment, not a fix for VOID's
private frontend or its fixed 1 MiB recovery contract. It was not adopted. No
LLVM SDK, stack reservation, frontend recursion guard or optimization setting
was changed.

The sentinel correction and original three cases are verified, but these two
independent Debug frontend failures prevent claiming that all mandatory compiler
regression gates passed. Further frontend stack repair lies outside this
assignment's minimum LLVM-backend bookkeeping scope and requires Boss direction.
G75 feature work has not resumed. This is the existing continuation gate failing
on a concrete compatibility defect, not a new milestone hold or a Release pass
excusing Debug.

## Regression run management

The first Release sweep completed all 503 registrations. Its original failures
are retained. Corrected fixture/security cases are replayed through the same
registered commands and original assertions; their initial failures are not
overwritten. The full Debug and strict sweeps use their respective native test
registrations (516 and 493). Long application/TLS/GC suites run at original limits.

After preserving 23 completed Debug passes and 14 completed strict passes, those
two queues were intentionally interrupted to increase qualification parallelism.
Every failed, interrupted or unrun case is in an explicit remaining-test list
(493 Debug, 479 strict). The replacement queues use CTest's documented
[`--tests-from-file` and parallel execution](https://cmake.org/cmake/help/latest/manual/ctest.1.html)
with same-filesystem scratch. The completed-pass sets plus the remaining lists
cover every original test name exactly; completed failures are included again.
Previously passing consumers of corrected fixture inputs are also confirmed
against the final inputs: twelve Release registrations and the two Debug
channel source/cache/stack registrations. G06 Debug/strict registrations are replayed
under private diagnostic roots.

Task-owned processes from the old qualification sessions were stopped before
replacement queues started. This is recorded as an interrupted qualification
attempt, not a compiler pass or a hidden application failure. The raw stdout logs
and named JUnit reports, rather than CTest's shared temporary last-run metadata,
are the authoritative per-run records.

## LLVM and artifact correctness

The original G75 Release compiler emitted 24 objects before the backend edit:
all eleven allocation-elimination fixtures, including the new negative-control
fixture, plus the exact legacy/minimal source, each in O0 and O2 mode. The
repaired Debug and Release compilers emitted the same inputs with matching
source hashes and options. **All 24 objects from each repaired compiler are
byte-identical to the pre-repair Release objects.** This checks complete artifact
bytes, not selected instruction counts. Compiler binary/build identities change
normally on rebuild; they are recorded separately from these object hashes.

The existing LLVM verifier still runs before optimization and after O2; all
these emissions passed it without changing verification or pass configuration.
The linked allocation tests additionally check execution, construction counts,
roots, failures and GC behavior. This is bounded fixture evidence, not a proof
about every possible program or an assertion that all host build profiles have
identical sanitizer metadata.

## Qualification limits and continuation

All execution in this repair is native **Linux x64**, using the existing pinned
LLVM/Clang/LLD **23.1.4** SDK and active verified providers. Windows and macOS are
**UNQUALIFIED**: no native run on either platform took place. Existing native
platform restrictions, installed tools, settings and recovery inputs were kept.

Debug retains its actual libstdc++ indexed-access assertion checks; a Release or
strict pass does not excuse a Debug failure. Strict is the existing
RelWithDebInfo ASan/UBSan build, with fatal reports and leak detection enabled.
Its LLVM SDK ABI has RTTI disabled, so vptr sanitization is unavailable. Compiler,
runtime and applicable native fixture instrumentation do not imply that every
third-party SDK instruction or generated JIT load/store has full sanitizer
instrumentation; existing generated-entry function metadata remains intact.

The full-suite onboarding example mismatch is an independent, unchanged static
documentation failure. Its test, README, first-program guide, hello source and
canonical learning fixtures all match HEAD; one first-program snippet diverges
from the canonical list. The assertion happens before the compiler is launched.
No unrelated guide edit or weakened example oracle is included. Therefore this
report does not claim an entirely green repository sweep.

No G75 feature work was resumed during this assignment. The original G75 JSON
report performance cost still needs Boss acceptance; the historical disabled/
enabled measurements are retained and are not relabeled as measurements of the
repaired compiler. Independent Wrench review and hosted acceptance remain
outstanding. G69's existing hold and G76's queued state are unchanged. No new
milestone hold was introduced. The concrete Debug frontend failures above keep
the existing mandatory continuation gate unmet.

## Exact changed-file inventory

The bounded repair touches **22 paths**: one production backend, CMake fixture
registration, the focused regression and existing test/probe inputs needed for
the required regression gates, plus documentation. Eighteen are additional to
the original 36-path G75 feature inventory; four overlap it. The complete working
tree contains **54 paths: 39 modified and 15 new**. The original feature work
remains unstaged and preserved.

| Path | Relation to original G75 inventory |
| --- | --- |
| `CMakeLists.txt` | Existing G75 path |
| `docs/README.md` | Existing G75 path |
| `docs/projects/G75_CLI_EXPERIENCE_IMPLEMENTATION.md` | Existing G75 path |
| `docs/projects/G75_DEBUG_CORRECTNESS_REPAIR.md` | Additional repair path |
| `examples/composition/order.void` | Additional repair path |
| `examples/composition/receiver_order.void` | Additional repair path |
| `examples/composition/stress.void` | Additional repair path |
| `examples/interfaces/handoffs.void` | Additional repair path |
| `examples/interfaces/stress.void` | Additional repair path |
| `examples/managed_initialization/stress.void` | Additional repair path |
| `examples/reference_values/order.void` | Additional repair path |
| `examples/reference_values/required.void` | Additional repair path |
| `examples/reference_values/stress.void` | Additional repair path |
| `src/backend/llvm/backend.cpp` | Existing G75 path |
| `tests/allocation_elimination/cases.hpp` | Additional repair path |
| `tests/allocation_elimination/fixtures/terminator_escape.void` | Additional repair path |
| `tests/binary/ownership.void` | Additional repair path |
| `tests/channel/reload.void` | Additional repair path |
| `tests/log/host.cpp` | Additional repair path |
| `tests/math/host.cpp` | Additional repair path |
| `tests/memory/ownership.void` | Additional repair path |
| `tests/stage10/console.cpp` | Additional repair path |

The [implementation report](G75_CLI_EXPERIENCE_IMPLEMENTATION.md#exact-changed-file-inventory)
lists every one of the 54 paths. `changed-file-inventory.json` in the retained
evidence records both inventories explicitly.

## Reproduction and retention

Use the existing pinned LLVM 23.1.4 SDK/provider profiles, with all normal
configure-time verification enabled:

```sh
cmake --build build/llvm23-debug -j 8
cmake --build build/llvm23-release -j 8
cmake --build build/llvm23-strict -j 8
```

For each build, run the exact retained `debug-repro.void` through `voidc run`
with `--no-cache`, first with `--no-opt`, then without it. Expected process status
is 19 with empty stdout/stderr. Build that same file to an explicit fresh output
path in each mode and execute it; expected status is also 19.

```sh
ctest --test-dir build/llvm23-debug --output-on-failure -R '^(project_managed_native|project_manifest_legacy|project_mixed_native|allocation_elimination|allocation_elimination_aot_normal|allocation_elimination_aot_optimized)$'
ctest --test-dir build/llvm23-release --output-on-failure -R '^(project_managed_native|project_manifest_legacy|project_mixed_native|allocation_elimination|allocation_elimination_aot_normal|allocation_elimination_aot_optimized)$'
ASAN_OPTIONS=detect_leaks=1:abort_on_error=1 UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1 ctest --test-dir build/llvm23-strict --output-on-failure -R '^(project_managed_native|project_manifest_legacy|project_mixed_native|allocation_elimination|allocation_elimination_aot_normal|allocation_elimination_aot_optimized)$'
```

For full registered suites, use `ctest --test-dir build/PROFILE -j 4
--output-on-failure --output-junit /fresh/report.xml`, retaining the strict
environment above. Set `TMPDIR` to a fresh private directory on the same
filesystem as the build for the hard-link negative test. Create the retained diagnostic directory with mode 0700 before setting
`VOID_TEST_DIAGNOSTICS_DIR` if full child logs are needed. Do not replace the expected linker rejection with success.
Set `VOID_IO_SECOND_VOLUME` to an actual distinct filesystem for the cross-volume
rename check (the Linux replay uses tmpfs `/tmp` against the XFS build).
The original fault-injection, sanitizer and timeout limits remain enabled.

`run_minimals.py`, `capture_objects.py`, the explicit remaining-test lists,
comparison scripts and JSON receipts retain the actual argv, input hashes,
compiler identities and output/status evidence used here. Binary/large object
copies are retained locally where required or identified by hash; they are not
represented as source evidence.

A separate verified repair archive is retained outside Git:

`/home/shmellyorc/Documents/Void.Language-archives/g75-debug-correctness-repair-2026-11-18.zip`

SHA-256: `b7aefc17c3dbc4ff063067167bdf56d97529616ab06648b4e9350e4988b3646b`. It has 245 ZIP
members (244 selected files plus restoration manifest),
6,204,337 selected source bytes and 1,172,215 ZIP bytes,
using ordinary ZIP DEFLATE level 9. CRC, every member hash and an actual extracted
restoration's hashes passed. The manifest records original paths and both source
and member SHA-256; its sibling `.index.json` records the archive hash and
verification. Both maintained G75 reports remain outside the ZIP to avoid a
self-referential report/archive hash.

The archive retains the original three failures and baseline backtraces, minimal
and new sentinel fixtures, the exact three-line production diff, before-source
controls and failure logs, complete final JUnit/raw suite records, named replay
lists, final result ledger, object identities, the two new independent frontend
reproducers/controls, exact inventories and replay scripts. Original G75
qualification/performance evidence remains in its unchanged earlier archive.
Private signing/TLS keys, large objects, compiler binaries, SDK/provider copies
and user installations are excluded. Frozen binaries and unique failure roots
remain locally for independent review; their hashes are recorded.

No cleanup was performed during this repair. Immediately before archiving,
filesystem-wide available space was 10,839,789,568 bytes on `/tmp`
and 225,957,326,848 bytes on the checkout filesystem.
These are live desktop measurements, not claimed recovery. Active builds,
verified dependency inputs, installed tools, user settings, recovery copies and
unrelated files remain.

Final source/compiler hashes and all 54 working paths match their retained
inventories. HEAD and locally recorded `origin/main` remain
`fdcbeadbdd8a0011751b448afa9fe40e5151ab1f`; staged diff is empty.
`git diff --check` passes. Relative file targets in both maintained G75 reports
resolve. The independent Debug frontend blocker and native Windows/macOS
qualification limits remain explicit.
