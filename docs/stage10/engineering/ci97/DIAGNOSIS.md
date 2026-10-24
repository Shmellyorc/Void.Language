# Step 42: CI #97 native console repairs

24 October 2026. **Step 42 and Stage 10 INCOMPLETE. Golden Wrench review pending.**
No Git mutation, hosted dispatch, workflow edit, root README change or new Runtime
architecture. Boss retains commit/push and native verification control.

## Two distinct hosted observations

- [Main CI #97](https://github.com/Shmellyorc/Void.Language/actions/runs/40069548583),
  `96bac2ea8e4be31d574a5916118feab0ef6b2e7f`: Windows Debug/Release **194/194 each**;
  Ubuntu Debug/Release **199/199 each**, including the original 1 MiB frontend gates.
  macOS ARM64 Debug/Release **195/199 each**, the four named native-console failures.
  Ubuntu Sanitizers **198/199**, Escape instead of Up Arrow. Job API records,
  success excerpts and complete failing-job logs are retained here.
- [ThinLTO experiment #2](https://github.com/Shmellyorc/Void.Language/actions/runs/40075098927),
  `8485e6404596cf73deb55302ad3678a23cc0c6a4`: separate job records and the Ubuntu
  Debug failure log are retained. Ubuntu Debug reports **198/199**, with the same
  `1b` versus `1b5b41` assertion. Other experiment outcomes are recorded by the
  existing, unmodified `docs/VOID_ThinLTO_Report.md`; they are not main-CI results.

Current local HEAD is the experiment revision. A read-only comparison shows that
it differs from the CI #97 baseline only by the experimental workflow; all six
investigated baseline source files match their CI #97 Git blob identities.
The console failure exists in main CI as well as the experiment. No claim is made
that ThinLTO caused, repaired or cannot influence its timing. Neither workflow is
changed by this mission; no performance experiment was repeated.

## macOS: descriptor flags and terminal mode verification

Production borrowing was inspected: the POSIX adapter captures stdin/stdout/stderr
without dup, close or F_SETFL/F_SETFD on those borrowed descriptors. Its wake pipe
owns and closes its own two descriptors; Darwin's fcntl setters are confined to
that owned pipe. The initially-closed-input protection remains. The per-thread
SIGPIPE mask, pre-existing pending signal and native errno/cleanup paths are
unchanged. No speculative adapter flag mutation repair was added.

Apple's published XNU source explains two invalid equality assumptions:

1. [`dofilewrite`](https://github.com/apple-oss-distributions/xnu/blob/f6217f891ac0bb64f3d375211650a4c1ff8ca1ea/bsd/kern/sys_generic.c)
   sets the kernel `FWASWRITTEN` bit after successful progress.
   [`F_GETFL`](https://github.com/apple-oss-distributions/xnu/blob/f6217f891ac0bb64f3d375211650a4c1ff8ca1ea/bsd/kern/kern_descrip.c)
   exposes those fileglob flags. The private bit's value is
   [0x00010000](https://github.com/apple-oss-distributions/xnu/blob/f6217f891ac0bb64f3d375211650a4c1ff8ca1ea/bsd/sys/fcntl.h).
   Comparing every raw status bit before and after an ordinary host write can
   therefore fail without any F_SETFL call by Runtime. The test helper now permits
   only **addition** of that Darwin bookkeeping bit. Removal, access-mode,
   O_NONBLOCK, O_APPEND, F_GETFD/FD_CLOEXEC and native-error changes remain failures.
   A plain `write(1,"HOST",4)` control constructs no Runtime. Raw flags and errno
   snapshots are included in the native driver records, including failure output
   for the originally failing pipe cases. Linux still compares all bits exactly.
2. [`TIOCSETA` in tty.c](https://github.com/apple-oss-distributions/xnu/blob/f6217f891ac0bb64f3d375211650a4c1ff8ca1ea/bsd/kern/tty.c)
   adds `PENDIN` on a non-flushing transition back to ICANON. The resulting state
   tells the line discipline to reprocess pending raw input. Runtime's previous
   equality predicate converted that successful kernel transition into a synthetic
   EIO, including nested mode setup and restoration. On Apple only, verification
   now excludes this transient bit while checking every other local flag, all
   input/output/control flags, control characters and speeds. Saved snapshots,
   LIFO ownership, actual tcsetattr/tcgetattr errors and cleanup errors are retained.
   Neither pending input nor the kernel bit is cleared to force equality.

The Python key/source PTY checks use the same narrow comparison and keep both
raw termios snapshots; `restored_exactly` now reflects actual byte-for-byte
attribute equality separately from `restored_configuration`. Cases that never
change modes, including closed-input and canonical availability, remain exact.
Original key/event/UTF-8/output assertions are unchanged.

**Evidence boundary:** the original hosted logs do not contain numeric flag
snapshots or tcsetattr return/readback pairs. The XNU mechanisms and the old
predicate's false failure are source-demonstrated, but attribution of each hosted
EIO/flag delta still requires the supplied observer and repaired tests on native
macOS. This Linux session cannot report observed Darwin values or native success.
`verify-kernel-contracts.py` compiles the extracted production predicate's Apple
branch and checks the descriptor helper and Python comparator. The original
predicate rejects the modeled PENDIN transition; the repair accepts that state
while rejecting unrelated changes. These are models, not macOS execution.

`host_observe.cpp` independently records native write/tcgetattr/tcsetattr return
codes, errno, flags, cc and speeds. Local real-PTY Linux returns are
`[4,0,0,0,0,0]`, errno all zero. Actual full before/active/restored attributes are in
`host-linux.json`; descriptor snapshots are in its `.flags.json` sidecar. No
Darwin record has been invented.

## Ubuntu: buffered continuation incorrectly expired

The original native PTY driver passed **20/20** locally in both Debug and strict
sanitizers before the repair. Thus its hosted intermittency was not reproduced
by ordinary local repetitions. A diagnostic `--wrap=read` build using the unchanged
Runtime captured each entire **103-byte** key batch in a single native read
(0.00637 ms and 0.00520 ms in the two closed/unclosed cases). Both complete Up Arrow
sequences were already in the shared queue. The trace introduces no delays or
input injection and is diagnostic evidence, not a performance claim. Read
fragmentation was not observed in these two batches; existing Fake chunk sizes
1, 2 and 4096 retain fragmented-decoder coverage.

The decoder started its 30 ms continuation deadline before terminfo lookup and
checked that deadline before looking at the next queued byte. Lookup cost or
scheduler delay could therefore split a completely buffered arrow. The exact
hosted delay source is not established by the available logs.

A deterministic virtual-clock test advances 31 ms during capability lookup;
there is **no sleep**. It fails against both accepted Debug and strict Runtime
archives. The same four-byte native batch gives these diagnostic outputs:

```text
before: key=4 bytes=1b     native_batch=4 clock_ms=31
after:  key=5 bytes=1b5b41 native_batch=4 clock_ms=31
```

The repair inspects the shared queue first. Buffered continuation bytes are decoded
without another native wait; an expired deadline still prevents fetching an
unbuffered continuation. Cancellation remains effective even with queued bytes.
The companion regression proves that a one-byte native batch still returns Escape
on expiry and the next shared reader receives `[Aq` intact. The original 30 ms
bound, polling behavior, lone-Escape assertions and original key values remain.
The successful arrow case also checks that the following `q` and mode restoration
are preserved.

## Local validation and limits

- Isolated `build/step42-ci97-debug` and `build/step42-ci97-san` each pass **11/11**
  focused suites: six Runtime console/transfer/input/key/terminal/wide drivers and
  five original POSIX native endpoint/transfer/input/key/terminal drivers.
- Both pass **20/20** post-repair original native-keyboard repetitions.
- Both pass the original `console_keys.void` PTY fixture's exact output in normal
  and O2 **independently linked AOT** using the repaired Runtime archive. The
  emitting compiler is the accepted CI96 build; it was not relinked for this
  focused check. Fresh compiler/JIT and the entire stage10_console_aot suite were
  not rerun, and this is not full CMake or hosted-matrix acceptance.
- The isolated harness rebuilds console.cpp with the accepted configuration's
  pinned LLVM/Clang 19.1.7 flags and replaces only `console.cpp.o` in a copy of the
  accepted Runtime archive. Other archive members, original CI96/Step41 binaries,
  source stack limits and 1 MiB test implementations remain unchanged. Exact
  commands, archive-member comparisons and final binary/source hashes are retained.
- Strict options remain `detect_leaks=1:halt_on_error=1:exitcode=87` and
  `halt_on_error=1:print_stacktrace=1:exitcode=86`. The initial sandbox run hit the
  documented LSan/ptrace limitation; its raw log is retained. Accepted strict
  execution used the established untraced profile with no disabled checks.
- One in-progress strict driver compile emitted invalid-UTF8 diagnostics from
  `<built-in>`; its raw output and command are retained. The final frozen-source
  build in the established untraced profile succeeded. No cause or compiler fix
  is claimed; earlier Step41 compiler observations remain historical and intact.
  A new guard test initially assumed read_bytes fills its requested extent; it was
  corrected to three ordered read_char calls, preserving the complete-byte
  assertion. Its initial failure is retained separately from the original bug.
  Aggregation also caught a recorder filename collision; per-case records now use
  separate filenames, and only the affected bounded checks were repeated.

## Next native gate, controlled by Boss

Golden Wrench reviews `repairs.diff`, raw observations, source/acceptance limits and
`step42-results.json` before any hosted run. On a native macOS checkout, compile
and run the independent observer, then the original focused tests on the repaired
build (all six console drivers plus native transfer/input/key/terminal, and
stage10_console_aot). Capture raw descriptor/termios records and native return
codes on both Debug and Release. Check the real nested and abandoned/explicit
restoration paths, source JIT/normal/O2 AOT cleanup, SIGPIPE modes, closed stdin and
mixed/shared input. Full acceptance and source/compiler identity need native
results; Linux success cannot substitute.

```sh
clang++ -std=c++20 docs/stage10/engineering/ci97/host_observe.cpp -o /tmp/void-ci97-observe
python3 docs/stage10/engineering/ci97/observe.py /tmp/void-ci97-observe
ctest --test-dir build/Debug --output-on-failure -R '^(runtime_console.*|stage10_console_aot)$'
```

No next Step 43/44 work or hosted run is performed here. Stage 10 remains
**INCOMPLETE**.
