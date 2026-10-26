# Release async filesystem SIGSEGV — independent research handoff

26 October 2026. Baseline: `e24d3a95db03fd282c87a5d9b885471057e38798`.
**Research only. No repair implemented or established. Golden Wrench review next.**

Follow-up: [Boss-authorized bounded phase capture](PHASE_CAPTURE_2026-10-26.md)
completed five attempts (one user-reported, four detailed captures) without a
SIGSEGV. Normal/O2 invocation and both teardown boundaries returned in every
detailed capture. No phase-specific isolation followed; the original cause
remains unestablished. Earlier observations below retain their original scope.

## Conclusion

The original cause remains **unestablished**. There is no failing instruction,
fault address, core or backtrace from the original invocation. Independent source
review found substantial lifetime/synchronization protections, but no demonstrated
faulty access that explains this SIGSEGV. Passing retries do not establish a fix.

The strongest immediate investigation lead is an **in-process compilation/JIT
failure, including the second O2 compilation**, followed closely by a generated
root/storage-lifetime error in the large async fixture. This orders experiments,
not proven causal probabilities. The failed test's name does not establish that
filesystem execution was running when it crashed. There is insufficient evidence
to recommend a production repair or blame LLVM.

The smallest useful next step is the already validated, no-rebuild phase/crash
capture described below: unchanged Release runner, hashed reconstructed fixture, real ASLR,
compilation/invocation markers and all-thread SIGSEGV capture. Make the next failure
diagnosable before changing workers, GC, optimization or shutdown behavior.

## Evidence reviewed and reproduced

Read all 59 original files under `docs/io/` (576,914 bytes), including the complete
initial/final logs, debugger attempts, XML receipts, API audit, contract,
implementation history and handoff. Independently inspected the async scheduler,
filesystem kernel/ABI, native ledger, Runtime coordination, LLVM lowering,
frontend/IR changes, source `io` module and execution/ownership/native tests.

All 53 source-freeze hashes and seven protected-baseline hashes matched before
research execution. HEAD matched the requested baseline and the worktree was
clean. No source, build configuration, original evidence, Git history or hosted
CI was changed. New files are this report and its separate research receipt.

| Observation | What it establishes |
|---|---|
| [Initial Release log](evidence/release-initial-io.txt), lines 25–41; [summary](evidence/release-initial-io-summary.txt) | `io_async_execution` failed after 55.73 s: runner return `-11`, empty captured stdout/stderr. The source check immediately before the runner had succeeded. The selection finished 26/27. |
| [Debugger attempt 1](evidence/release-gdb-attempt-1.txt) and [attempt 2](evidence/release-gdb-attempt-2.txt) | Both passes completed; four worker threads were created per pass. No crash stack. Logs do not contain the original runner's hash or a failing PC. |
| [Final receipt](evidence/RECEIPT.md) | Later Debug 63/63, Release 63/63 and strict sanitizer 56/56 selections passed. They remain retries after intervening work, not original first-attempt success. |
| Reconstructed fixture at the reported pathname | `/tmp/VOID io async wire 1icf1laa/main.void` is a reconstruction from the final committed generator, **not a preserved original crash file**. It has 17,032 bytes, SHA-256 `64b6a7abbf71eb627d80a0244acb9a0db061e5f6c6f5d582e38732d510befaab`. Neither original bytes nor an original fixture hash were archived. |
| Two new direct Release invocations | Both returned 0, in 63.375 s and 61.401 s, with normal and O2 success messages and empty stderr. No crash/core. |
| One new phase-traced GDB invocation | Both passes returned normally with ASLR enabled. Compilation/invocation markers worked without rebuilding. No crash stack. |
| Independent output checks | All six resulting little/big-endian files matched the Python oracle, across the two direct runs and the debugger run; each uses all 42 numeric vectors plus Pascal/raw text. |

New results, exact hashes and the debugger transcript are in
[the research receipt](evidence/sigsegv-research-2026-10-26.json).
The existing runner's SHA-256 is
`b42d5bcd782470be46a5a6009a56b8003fef5e8439e2b93caa04f0b027f3a3b6`.
Its current modification time is 09:20, after the initial 08:56 selection began. The record has no
hash-preserved copy of the original failing executable; current reruns therefore
test the final frozen baseline, not a demonstrably identical historical binary.

New direct runs used fresh temporary **native working directories** while keeping
the reconstructed source at the reported pathname. This retains the candidate's
source/import path identity, without proving historical source-byte identity or
overwriting existing filesystem outputs. The candidate exercises relative native
paths in its native working directory. All three research runs were candidate
executions; none was verified execution of the original crashing bytes.

### Additional Boss-supplied reconstruction and original-log comparison

Boss clarified that the original command was:

```text
void_io_runner --async "/tmp/VOID io async wire 1icf1laa/main.void"
```

The original temporary source was **not preserved**. Earlier wording in this report
calling the current file “retained” has been corrected. The generator could have
changed during subsequent repairs, so matching today's generator cannot recover
the missing provenance.

Inspected `/home/shmellyorc/Downloads/VOID_async_SIGSEGV_reproducer.zip` in place,
without executing its contents or treating its README as task instructions. It
contains `main.void`, `README_REPRO.md` and `RELEASE_FAILURE_EXCERPT.txt`.
Archive SHA-256:
`c08dfb63167eb1babfab68a8c4292e375cf1f764a14f7ab1503d748e1d6c4827`.

The archive's source, `/home/shmellyorc/Downloads/main.void`, the reconstructed
`/tmp` copy and `async_fixture()` from the committed baseline generator all match
byte for byte. The archived assertion line also matches both original full and
summary failure logs. This supplies a concrete, repeatable **candidate workload**
and corroborates the reported command/exit status. It supplies no original source
capture, failing stack or new crash reproduction, and does not change the causal
conclusions above. No additional execution was needed to verify identical bytes.

Independent subagent review of the archive alongside the original logs identified:

- 122 syntactic await sites: 93 in `wire`, 28 in `exercise`, one in `entry`.
  `exercise` awaits little-endian `wire` and then big-endian `wire` sequentially.
  These are source-site counts, not a total including repeated calls or stdlib
  helper awaits.
- `wire` includes 42 numeric writes and 42 reads, ten numeric generic types,
  repeated byte-array encoding/comparison, Pascal/raw text and checked cleanup.
  This is a concrete large-frame/root-map and optimizer stress case.
- No source `spawn`, explicit cancellation or parallel branches. Each written
  filesystem call is immediately awaited. Four default native workers can service
  successive jobs, but this is not a test deliberately issuing four simultaneous
  operations against one file. Multiworker concerns remain completion/GC-rendezvous
  hypotheses rather than an assumed same-handle race.

No new faulty access was established by this package. Preserve the large candidate
for capture before reducing it; its exact historical fidelity remains unknown.

### Why empty stdout does not identify the crash phase

[`tests/io/runner.cpp:76–82`](../../tests/io/runner.cpp#L76) constructs a fresh JIT,
compiles and invokes unoptimized source, then repeats with O2. Its success line
ends with `\n`, without an explicit flush.
[`execution.py:21–24`](../../tests/io/execution.py#L21) captures stdout through a
pipe. The first success line may still be buffered when the second compilation,
execution or destruction fails. Thus `b''` does not establish a first-pass crash.
Release describes the C++ host build; it does not mean that only O2 source ran.

The new debugger run measured:

| Marker | Seconds from debugger start |
|---|---:|
| First `Jit::compile_async` | 0.241 |
| First `NativeCode::invoke` / `vd_async_root` | 22.292 |
| Second `Jit::compile_async` | 22.376 |
| Second `NativeCode::invoke` / `vd_async_root` | 58.672 |

Compilation occupied almost all elapsed time in this passing run. This makes
phase capture particularly valuable; it does **not** identify the phase of the
historical 55.73-second failure. CPU load, paths and scheduling differed.

## Ranked hypotheses

Confidence below means confidence **as the cause of the original failure**.
All remain low or very low because none has a failing access. Rank expresses
investigation value, taking supporting evidence and counterevidence together.

### 1. In-process O2 compilation/ORC failure — low causal confidence

Exact seams: [`runner.cpp:76–80`](../../tests/io/runner.cpp#L76),
[`backend.cpp:1855–1860`](../../src/backend/llvm/backend.cpp#L1855), and
[`finish():73–90`](../../src/backend/llvm/backend.cpp#L73).
The second pass lowers a fresh module, runs the ordinary LLVM O2 pipeline, loads
it through ORC, then invokes it. The buffered-output observation permits a crash
anywhere along this path. The new timing evidence shows why an apparent long
“execution test” may mostly be compilation.

A concrete, separate historical observation exists:
[Stage 11's retained stack](../stage11/evidence/llvm-dse-backtrace.txt), lines 4–29,
stopped in `CallBase::getMemoryEffects` / `DSEPass` during an O2 console compile
in a Debug host build.
That establishes an unresolved optimizer-phase failure in this codebase/toolchain,
not the cause of the I/O failure. Its [record](../stage11/OPTIMIZER_FAILURE.md)
also says serialized IR passed standalone `opt`, and serialization changed
reproducibility. It does not prove an upstream LLVM defect or sound in-process IR.

**Discriminator:** a new failing PC/backtrace during compile, versus after invoke.
If compilation is implicated, isolate O2 compilation without the preceding normal
source execution before blaming an optimizer pass: an earlier invalid generated
write could otherwise corrupt host state and fail later inside LLVM. Capture the
pre-optimization module and its verifier result only after locating this phase.
Use the pinned standalone tools to distinguish reproducible IR input from an
in-process state/lifetime problem. Do not disable DSE or change LLVM as a guess.

### 2. Generated root/storage lifetime across GC and await — low causal confidence

Exact seams:

- Source JIT creates `Runtime(256)` at
  [`backend.cpp:1788–1790`](../../src/backend/llvm/backend.cpp#L1788).
- Allocation-triggered collection is real:
  [`coordination.cpp:261–264`](../../src/runtime/coordination.cpp#L261) and
  [`buffer.cpp:113–114`](../../src/runtime/buffer.cpp#L113).
- Async root homes/registration:
  [`backend.cpp:335–340`](../../src/backend/llvm/backend.cpp#L335),
  [`707–726`](../../src/backend/llvm/backend.cpp#L707).
- Await restores prior frame values and copies aggregate results:
  [`1198–1208`](../../src/backend/llvm/backend.cpp#L1198); async result stores occur
  at [`1570`](../../src/backend/llvm/backend.cpp#L1570).
- Native reply roots/publication:
  [`filesystem_abi.cpp:124–191`](../../src/runtime/filesystem_abi.cpp#L124).

The full generated wire fixture has 42 numeric vectors, nested generic wrappers,
many awaits, collections and directory/path results. An omitted reference in a
generated frame, stale aggregate home, or wrong temporary lifetime could survive
small tests and fail when allocation triggers collection. This is a concrete
seam to inspect, **not an identified omission**. Do not substitute raw `Runtime`'s
default zero budget for the actual source-JIT budget of 256.

Counterevidence: frame maps and await stores are present; native reply fields
7–9 and metadata-entry path/name payloads are precisely rooted before allocation.
The heap is nonmoving, so collection does not by itself invalidate every cached
task address. Existing forced-GC tests are meaningful. However,
[`ownership.cpp:58–109`](../../tests/io/ownership.cpp#L58) runs the smaller
[`ownership_async.void`](../../tests/io/ownership_async.void), not the full wire
fixture. No missing root or dangling storage was demonstrated in this audit.

**Discriminator after execution is implicated:** run the full reconstructed candidate once
per mode with the existing root-inspection/forced-GC probe. Then, in a disposable
diagnostic harness using existing public APIs, compare collection budgets 256,
0 and 1 independently. Preserve workers/source/optimization otherwise. Correlation
narrows the problem; disabling GC is not a repair, and passing repetitions do not
disprove a rare lifetime error.

### 3. Multiworker completion/cancellation/rollback edge — low causal confidence

Exact seams: queue/dequeue
[`async.cpp:101,132–134`](../../src/runtime/async.cpp#L101), result handoff
[`120–140`](../../src/runtime/async.cpp#L120), owner publication/rollback
[`167–225`](../../src/runtime/async.cpp#L167), and submission
[`450–475`](../../src/runtime/async.cpp#L450).

Focused native proofs force one worker at
[`native_jobs.cpp:162`](../../tests/io/native_jobs.cpp#L162) and
[`native.cpp:34,117`](../../tests/io/native.cpp#L34). The default is four
([`runtime.hpp:168`](../../include/void/runtime.hpp#L168)), also visible in the
original debugger logs. One-worker gated cases do not force multiple workers
through completion, cancellation and acquisition rollback together.

Counterevidence is substantial: the queue uses a mutex; native result/failure
writes precede a release store to `Work::done`, and the owner uses acquire before
publishing. Stable heap-allocated `Task`s are owned through `unique_ptr`; vector
growth does not move the pointed-to tasks. Native publication is owner-only.
No data race or access after a handoff was found. The wire fixture awaits its
operations rather than intentionally racing aliases; cancellation is not injected
in the original test.

**Discriminator if a worker/handoff stack is implicated:** same full fixture,
four versus one worker, then controlled handoff gates using the existing native
hooks/semaphores. A difference is scheduling evidence, not proof of a repair.
Do not permanently serialize the executor to make the crash disappear.

### 4. Worker allocation/error-cleanup path under resource pressure — very low

Relevant sites: worker opening
[`filesystem_abi.cpp:235–251`](../../src/runtime/filesystem_abi.cpp#L235), native
file allocation/buffer construction
[`filesystem.cpp:205–217`](../../src/runtime/filesystem.cpp#L205), diagnostic
handoff [`async.cpp:120`](../../src/runtime/async.cpp#L120).

The 622-allocation sweep uses ordinary global `new` overrides with a **thread-local**
failure budget ([`probe.cpp:6–34`](../../tests/managed/probe.cpp#L6)). It covers
owner-side C++ allocations too, not just managed object constructors; it does not
propagate the injected budget into native worker threads, nor override every
aligned allocation form. Worker `File`, string/vector, directory-iteration and
diagnostic-allocation failure paths therefore lack equivalent deterministic
injection coverage.

Counterevidence: native job execution catches allocation failures and returns
checked failures. No OOM or injected allocation failure was recorded in the
original invocation. Ordinary resource exhaustion alone does not explain SIGSEGV;
an additional erroneous failure path would have to be demonstrated. Concurrent
suites may change pressure/timing, but that is not evidence of such a defect.

**Discriminator only if indicated:** inject worker-local allocation failure using
the existing hooks/test infrastructure at the implicated operation; check resource
retirement, admission balance and retained diagnostics. Avoid another broad owner
sweep that never reaches the worker allocation under suspicion.

### 5. Shutdown, file retirement or JIT-layout dangling — very low on this runner

Relevant sites: resource admission/drain
[`coordination.cpp:311–362`](../../src/runtime/coordination.cpp#L311),
[`filesystem_abi.cpp:195–225`](../../src/runtime/filesystem_abi.cpp#L195),
[`native_resource.cpp:82–104`](../../src/runtime/native_resource.cpp#L82).

The later shutdown repair addresses a demonstrated entered-open race, but the
original runner does not call `Runtime::close` concurrently with its invocation.
Normal teardown follows invocation and worker joining. Borrowed `ReplyLayout`
metadata is JIT-owned, but
[`NativeCode::invoke:39–45`](../../src/runtime/reload.cpp#L39) holds a generation
lease through the checked call; the async scope joins workers before destroying
tasks ([`async.cpp:89–92`](../../src/runtime/async.cpp#L89)).

Jobs retain owned native requests/results and shared file/resource references.
[`FileResource::begin/end/close:78–111`](../../src/runtime/filesystem_abi.cpp#L78)
exclude overlapping operations and wait for active work before shutdown closes.
The managed identity has a persistent handle, and fd retirement uses a one-shot
exchange ([`filesystem.cpp:618–634`](../../src/runtime/filesystem.cpp#L618)).
These protections contradict the simplest “task/file freed while worker uses it”
story. They are not a formal proof of every exceptional path.

### 6. Fixed filesystem ABI geometry mismatch — very low

[`filesystem.inc:17–40`](../../src/backend/llvm/filesystem.inc#L17) obtains reply
geometry from the target LLVM DataLayout and checks record size/key offsets.
[`src/ir/filesystem.inc:46–90`](../../src/ir/filesystem.inc#L46) independently
checks reply/entry field shapes; the native publisher uses matching scalar,
boolean and reference fields. No incompatible layout or invalid fixed offset
was found. Repeated execution of both modes and independent linked AOT replies
provide counterevidence. A malformed pointer or dynamic storage lifetime can
still fail despite a correct static layout; that belongs to hypotheses 2/3.

## What changed between the initial failure and final success?

| Change or observation | Supported conclusion |
|---|---|
| Initial Release ownership already reports 622 recovered async failures | The budget-580 close-reservation correction was already reflected in that selection. It cannot be cited as a later explanation for the SIGSEGV. |
| Entered-open/whole-file shutdown admission repair | Real later defect/repair, with retained reproducer and new tests. It protects Runtime close through publication/rollback; no causal link to this runner's crash was captured. |
| Explicit-base path resolution avoids an unnecessary cwd query | Documented path correction. No evidence it repairs invalid memory in the failed runner. |
| Final CTest selection includes two additional shutdown tests and different ordering | Execution environment/scheduling changed. Initial async execution started near ownership and sync execution; final async execution started near the end. This can affect timing/pressure. |
| Current runner postdates the initial selection; final source freeze recorded | Intervening repairs required a new final build, but the initial binary/source state was not preserved with hashes. The evidence does not support an exact binary/source diff at the failing instant. |
| Standalone debugger retries and final passing selection | Demonstrate successful executions, not that the original crash was repaired. |

Earlier development logs also retain transient source type errors while a repair
was in progress. Those were checked diagnostics, distinct from `-11`. The initial
Release async test reached its runner after source checking. There is no evidence
that an active source edit caused that SIGSEGV, and no original source snapshot to
settle the question completely. Keep the existing successful final source freeze
and historical failure evidence separate.

Concurrent CTests execute separate processes. `execution.py:204–209` gives each
async case its own temporary directory. Their `Task`s, Runtime heaps, TLS and file
resources are not shared. No direct same-resource cross-test race was identified.
Machine load and memory pressure remain uncontrolled differences, not a concrete
shared-object explanation. The current shell has core limit 0 and `core_pattern`
is `core`; the original process's limits were not recorded. No original core was
found beside the reconstructed fixture or Release build.

## Test coverage and sanitizer limits

The suite covers normal/O2 execution, independent AOT, roots/allocation sweeps,
queued/entered cancellation, owner responsiveness, publication failure, acquisition
rollback, close errors, pending close, shutdown and entered opening/whole-file
shutdown. Those are substantial protections; they do **not** cover every suspected
path above.

Important remaining gaps are the full large fixture under deterministic root
inspection, controlled multiworker handoffs, worker-local allocation failure,
and first-failure phase/core capture. Existing normal execution with four workers
is different from a deliberately gated four-worker race test.

Strict ASan/UBSan covers C++ compiler/Runtime/test code and interceptable allocation
and library calls. It is not blanket instrumentation of generated VOID instructions:
[`CMakeLists.txt:66–68`](../../CMakeLists.txt#L66) adds host compile/link flags;
[`finish():73–90`](../../src/backend/llvm/backend.cpp#L73) adds no generated-IR
ASan/UBSan pass. Existing `func_sanitize` metadata supports indirect-call type
checking, not every JIT load/store or arithmetic operation. Prebuilt LLVM internals
are not retroactively instrumented either. This is consistent with
[Clang 19.1's instrumentation/runtime model](https://releases.llvm.org/19.1.0/tools/clang/docs/AddressSanitizer.html).
The 56/56 result therefore cannot rule out an invalid direct generated load/store,
or a schedule not exercised in that run. No sanitizer was disabled for this research.

## Smallest practical next experiment

**Continue the validated no-code first-failure capture, bounded in count/time.**
Preserve the frozen runner, candidate source and stdlib hashes. Use the same full reconstructed candidate,
default workers and normal GC policy. Fresh native working directories prevent
test outputs from overwriting prior evidence. Run a small recorded batch of direct
invocations with a child-only core limit, stopping on the first abnormal exit.
Do not change the system-wide core pattern. The two direct attempts here used a
1 GiB child core limit and 180-second timeout; neither failed.

For an attached diagnostic run, the following settings and symbol breakpoints
worked on the existing Release binary, which has symbols but no DWARF source-line
tables:

```text
set pagination off
set confirm off
set disable-randomization off
handle SIGSEGV stop print nopass
break 'vd::Jit::compile_async(vd::ValidatedAnalysis const&, bool, vd::Compatibility)'
break 'vd::NativeCode::invoke(int) const'
break vd_async_root
```

Use silent breakpoint commands with a counter/timestamp and automatic `continue`;
the research receipt shows the resulting markers. On SIGSEGV, **stop rather than
retry**, then capture:

```text
thread apply all bt full 40
info registers
x/24i $pc-24
info proc mappings
info sharedlibrary
generate-core-file <private-research-directory>/crash.core
```

The first sandboxed GDB launch was blocked by ptrace before source execution.
The authorized debugger run outside that sandbox succeeded. Keep ASLR enabled;
debugger timing still perturbs execution, so retain ordinary core-enabled attempts
alongside it. [GDB signal handling](https://sourceware.org/gdb/current/onlinedocs/gdb.html/Signals.html)
and [inferior startup/environment](https://sourceware.org/gdb/current/onlinedocs/gdb.html/Starting.html)
describe these controls. A normal exit is another non-reproduction, not a captured
failure or proof that a hypothesis is false.

After a failure selects a phase, change **one** variable:

1. Compile/optimizer/ORC stack: isolate compilation from prior generated execution,
   retain prepass IR/object provenance and use pinned 19.1.7 tools. The existing
   runner already supplies a smaller discriminator without adding a harness:
   `void_io_runner --emit-async <candidate-main.void> <temporary-output.o> optimized`
   (`tests/io/runner.cpp:68–73`, `backend.cpp:2153`). It checks/lowers/optimizes/emits
   the candidate without invoking generated code or launching native filesystem
   jobs. A captured failure there localizes to compilation/emission rather than
   that run's native publication. A pass does not exclude intermittent compiler
   failure or ORC-specific behavior; AOT emission is not identical to JIT loading.
   Pair it with phase capture, without speculative optimizer disable or LLVM fork.
2. Generated execution/GC stack: reuse the existing ownership probe for one full
   wire invocation per mode; then compare GC budgets 256/0/1 in a disposable
   diagnostic harness if needed. Follow `compile_analysis()`'s existing cold path
   (`backend.cpp:1872`): lower the analysis, set `module.async_root`, and compile
   with an explicit Runtime. This is an experiment, not a shipped GC change.
3. Worker/publication stack: compare four/one workers with other inputs fixed,
   then use existing gates to force the implicated handoff. Do not infer a race
   merely because serialization changes failure frequency.
4. Teardown stack: capture the last pending job, resource admission, generation
   owner and thread states before proposing a shutdown/lifetime change.

A stopped PC is necessary localization, not always root cause: earlier memory
corruption can fail later in an innocent component. Require the implicated invariant
or minimal failing input/schedule before recommending a repair.

### Workload reduction after a failing phase is captured

The supplied candidate now gives precise reduction boundaries. First separate
the two sequential wire calls (`main.void:276–279`) from the filesystem/error/path
tail (`280–345`). If wire reproduces, compare one wire call versus two before
changing its 42-vector unrolled body; then remove corresponding write/read vector
blocks in pairs. Preserve the floating decode/`Ok`/await branches, `with` acquisition
and explicit async close while testing them. Replacing the unrolled awaits with a
loop immediately changes the activation layout, resume switch and optimizer input.

If the tail reproduces, inspect managed error plus partial-byte payload handling
(`291–297`) and metadata-result arrays (`298–302`) before the later path operations.
These are proposed reductions of the reconstructed candidate, not tested smaller
reproducers or proof that those operations are faulty. Changing entry reachability
can also change compilation/optimization, so interpret a reduction against the
captured failing phase and preserve the original full candidate as a control.

## Review disposition

**Confirmed root cause: none.** Evidence-supported leads and explicit test gaps are
ranked above. A direct file/task UAF, shutdown fix, static ABI mismatch, OOM event,
upstream LLVM defect or cross-test shared-memory race is not established.

Initial research evidence was three successful runner invocations (two direct, one GDB)
and six matching independent wire-output checks, with no new source build, hosted
CI or production modifications. The historical
failure remains open. Golden Wrench should review this report and the minimal
capture plan; Keyboard should implement a repair only after Boss authorizes a
specific evidence-supported change. The subsequent authorized bounded capture is
recorded separately in [its handoff](PHASE_CAPTURE_2026-10-26.md).
