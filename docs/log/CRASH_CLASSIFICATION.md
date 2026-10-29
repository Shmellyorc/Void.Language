# Bounded native crash classification

Follow-up: [RESIDENT_TEXT_CAPTURE.md](RESIDENT_TEXT_CAPTURE.md) records the separately
authorized `0x37` capture attempt. The 12-launch bound ended without a new core
(11 passes; one launcher failure with unknown native exit). The resident-text gap
and unresolved disposition below remain. The original report follows unchanged.

28 October 2026. Baseline `db808efe0fa191ed25733513cc9ebcebbadc3bc2`.

**Disposition D: root cause unresolved. Faulting component identified as LLVM
optimization inside VOIDC's JIT compilation path. STOP for Golden Wrench review.**
Performance gate OPEN; release HOLD. `text.translate_scalars` remains architecturally
approved, but its implementation has not resumed. No production correctness repair
is justified by the evidence obtained so far.

## Preserved identity and bounded execution

The original executable SHA-256 is
`c284712dd4958faf41547deb49a851306256a9075cf59d968671272bc17e96bb`.
All **61 original source inventory entries** were verified against both their live
paths and preserved copies before execution. The original incidents
`incident-vs4b96zy` and `incident-tg6xt8tp` remain unchanged.

For the following paths, `H` denotes the literal external evidence root
`/home/shmellyorc/.local/state/void-log-hotpath/db808efe-20261028` and `N` denotes
`H/native-core-classification`. These abbreviations describe paths; they were not
substituted into the actual recorded command. Exact argv:

```text
/home/shmellyorc/.local/state/void-log-hotpath/db808efe-20261028/combined-keys/void_log_host-timing
/home/shmellyorc/.local/state/void-log-hotpath/db808efe-20261028/flat-builder/performance.void
/tmp/VOID-hotpath-combined-kdkc1i_d
optimized
timing
```

Launch cwd: `/home/shmellyorc/Documents/Void.Language`. The host subsequently uses
the third argument as its working directory, with the same pathname spelling.
Explicit workload environment:

```text
VOID_STDLIB_ROOT=/home/shmellyorc/.local/state/void-log-hotpath/db808efe-20261028/flat-builder/stdlib
VOID_PERF_SHAPES=0,4,9
VOID_PERF_STAGES=6,7,11,12
```

The original full environment was **not retained**. Source, binary, command, cwd,
mode and recorded explicit filters match; perfect environmental identity is not
claimed. `N/plan.json` records relevant current environment values, including absent
`LD_PRELOAD`, `LD_AUDIT`, `LD_LIBRARY_PATH`, sanitizer and allocator options.
Crash-recorder evidence is directed to `N/crashes`.

Before launching, the investigation found no core in either working directory,
no files in `/var/lib/systemd/coredump`, and no journal coredump entry for the
original executable. `coredumpctl` is unavailable. The inherited configuration was
`core_pattern=core`, `core_uses_pid=1`, `suid_dumpable=0`, core soft limit zero,
core hard limit unlimited, and `coredump_filter=0x33`. It permitted direct native
core capture without rebuilding, injecting a library or attaching a debugger.
The exact inspection is in `N/core-feasibility.json`.

The child pre-exec hook retained **soft and hard execution stack limits of
1,048,576 bytes**, enabled an unlimited process-local core limit, and used umask
0077. The existing task-owned `/tmp/VOID-hotpath-combined-kdkc1i_d` directory was
made private (0775 → 0700). No system-wide kernel setting or privilege changed.

| Original-command attempt | Return status | Observation |
|---|---:|---|
| 1–9 | 0 each | Nine completed invocations, 432 diagnostic rows including warmups |
| 10 | -11 / SIGSEGV | Empty stdout/stderr; native core captured; launches stopped |
| 11–12 | Not attempted | First captured crash ended the authorized run sequence |

All attempts used `tests/log/support.py` and the existing
`tests/text_encoding/crash.py` workflow. Failed attempt 10 is preserved as
**`N/crashes/incident-w415alhc`**, with receipt `N/receipts/run-4bjlcahz`.
Passing attempts are observations, not evidence of repair. No secondary
instrumentation variant was needed or built.

Core path: `N/cores/original-core-10-core.1682824`.
Size: **120,406,016 bytes**. SHA-256:
`2c313fd4d8fba185efdceb35f519a5f6d8bc3d80e47e37b3ed367569405aad24`.
The original core at `/tmp/VOID-hotpath-combined-kdkc1i_d/core.1682824` is retained,
byte-identical, mode 0600. The external evidence directory is 0700; cores and
preserved executable/library files are 0600. Raw memory stays outside Git and
outside the compact review archive.

## Captured fault and phase

Offline GDB 17.2, elfutils and GNU objdump/readelf inspected the core. GDB startup
files, auto-load and debuginfod were disabled. Commands load an executable and
core only; none runs or attaches to an inferior. Commands, outputs, return codes
and tool hashes are retained in `N/receipts`, `offline-commands.json`,
`identities.json` and the copied analysis drivers.

| Field | Captured value |
|---|---|
| Failing thread | Sole captured LWP 1682824 |
| Signal / code | SIGSEGV (11), `si_code=128` (`SI_KERNEL` in local Linux headers) |
| `si_addr` field | 0; this does **not** establish a null-pointer dereference |
| Kernel diagnosis | General-protection fault, x86 error code 0 |
| RIP | `0x5565df5f470e` |
| RSP | `0x7ffd8c5ec580` |
| Executable load bias | `0x5565dd4a4000` |
| ELF virtual address / file offset | `0x215070e` / `0x214f70e` |
| Symbol | `eliminateDeadStores(...)`, local LLVM clone, offset +6686 |
| Mapped execution stack | `[0x7ffd8c5bc000, 0x7ffd8c5f3000)`; 225,280 bytes |
| RSP above mapped stack low | 198,016 bytes |
| Stack top minus RSP | 27,264 bytes |
| Nominal 1 MiB floor | `0x7ffd8c4f3000`; RSP is 1,021,312 bytes above it |

The nominal floor is calculated from mapped stack top and the enforced limit; it
is not a captured guard-page mapping. This state does not support exhaustion of
the execution stack. It does not prove that no earlier memory corruption occurred.
Full registers and mappings are in `gdb-initial.stdout` and `core-notes.txt`.
Notable registers: `rax=6`, `r13=0x5566033d42b0`, `r12=28`,
`rcx=0x5565dd944d5c`, `eflags=0x10216`, `cs=0x33`, `ss=0x2b`.

The failing unwind, abbreviated without changing order:

```text
eliminateDeadStores(...)
llvm::DSEPass::run(...)
function / CGSCC / module pass managers and inliner adaptors
vd::(anonymous namespace)::finish(llvm::Module&, bool)
vd::Jit::compile(...)
(anonymous namespace)::compile(...) [preserved benchmark host]
main
```

The PC belongs to LLVM 19.1.7 code linked into the original host executable.
It is **not an address in generated JIT code**. In `src/backend/llvm/backend.cpp`,
`finish` verifies the LLVM module before running the O2 pass pipeline; `Jit::compile`
calls `finish` before `impl_->load`. Together with the stack, this places the
captured failure after source analysis/lowering and the pre-optimization verifier,
inside optimization, before this workload's machine-code emission/linkage,
`perf_setup`, timed `perf_stage`, checksum and first output row. The post-optimization
verifier was not reached. A successful pre-verification does not exclude invalid
in-process state or a later transformation defect.

There is no generated-code PC to correlate with an ORC object or generation.
No JIT source mapping is invented. No exact LLVM source line, offending IR
instruction or invalid IR invariant was recovered: this executable lacks the
required local-variable/line debug information. The preserved symbol and native
disassembly are the available location evidence.

## Instruction evidence and its limits

Function-aligned GDB disassembly and independent objdump agree on these **on-disk**
bytes at the normalized fault location:

```text
0x2150709  41 0f b7 45 02   movzwl 0x2(%r13),%eax
0x215070e  c1 e8 04         shr    $0x4,%eax
0x2150711  83 e0 38         and    $0x38,%eax
0x2150714  48 8d 0d ...     lea    ...,%rcx
0x215071b  80 7c 08 02 00   cmpb   $0x0,0x2(%rax,%rcx,1)
```

The recorded PC corresponds to a register-only shift, not a memory access whose
invalid effective address has been demonstrated. The preceding load's operand
is present in the core: the word at `r13+2` is 6, consistent with saved EAX.
That observation does not establish the validity or lifetime of the LLVM object.

The same PID, RIP and RSP appear in the kernel journal at 12:48:13 local time,
explicitly reporting a general-protection fault with error code 0. This is
stronger evidence than assuming a page fault from the signal name. However,
the core's executable-text PT_LOAD segment has **`p_filesz=0`** under the inherited
filter. GDB obtains these instruction bytes from the preserved file, not from
captured resident text. The record therefore does not prove which bytes the CPU
actually executed, or why the general-protection fault arose. No hardware,
kernel, LLVM or preceding memory-corruption cause is declared proven.

The first exploratory `x/i` range began mid-instruction and displayed `(bad)`
decodings. The subsequent function-aligned disassembly supersedes that display;
the initial output is retained and is not evidence of corrupted instructions.

Two earlier kernel records at 10:55:30 and 11:39:20 correspond in time and
executable name/range to the original incident receipts (mtime 10:55:30.581 and
11:39:21.003). Their normalized ELF PCs are `0x21506be` and `0x21505df`, also in
`eliminateDeadStores`: on disk, `add %rcx,%rax` and `movdqa 0x7d0(%rsp),%xmm1`.
Both journal records report general-protection faults, error 0. The second
record's RSP plus 0x7d0 is 16-byte aligned, so that record alone does not establish
an unaligned-SIMD-load defect. These are useful correlations, not recovered old
cores: the old recorder did not retain PID or timestamp fields to bind them as
strongly as the new core. No common causal invariant is proven across the three.

## Executable, library and debugger identity

The core's `AT_EXECFN`, NT_FILE paths, ELF program headers and entry address match
the preserved executable. Its captured first 4,096 bytes match the executable
byte for byte; the full on-disk SHA-256 remains the original required identity.
The binary has no GNU build ID. All seven loaded file-backed library first pages
also match their preserved files, and all seven build IDs match those extracted
from the core. Exact SHA-256, full paths and private copies are in
`N/identities.json`, `identity-verification.json` and `identities/`.

| Loaded library | Core/file build ID |
|---|---|
| libc.so.6 | `a6ece0eae08c71f58eaefb817ae72b57782127d3` |
| libgcc_s.so.1 | `b1eb168258a31c6ec0f94a1c334199b86b90ce53` |
| libstdc++.so.6.0.36 | `821da2c5c65859c6bf8b4af0dbf40a70bede6241` |
| libzstd.so.1.5.7 | `b87df161f2bebb52390c97646b8860b5da7f6482` |
| libz.so.1.3.2 | `fd46e4c7b2ee9d7d49dfb9dd7bd13974a8e65cac` |
| libm.so.6 | `1900f789081f5b94c8a79d0180bf42e8dd064e93` |
| ld-linux-x86-64.so.2 | `63a341333cb5334af802e065c3d535767403fc43` |
| Kernel vDSO, extracted from core | `832c472e19535ff369da3991ee738ca66bf13aee` |

GDB emits `core file may not match specified executable file`. This warning is
preserved, not suppressed. An offline same-bytes symlink alias using the truncated
PRPSINFO basename did not remove it. Upstream GDB's
[matching test](https://gnu.googlesource.com/binutils-gdb/+/refs/heads/master/gdb/testsuite/gdb.base/corefile-exec-mismatch.exp)
documents a warning case involving an executable without a build ID and libraries
with build IDs. That is relevant context, not a verified explanation of this
installed GDB's warning. The independent identity checks above support the symbol
mapping; they do not verify omitted resident text pages. The precise warning
mechanism remains unconfirmed.

GDB SHA-256: `958009c3e3fe0e856b7646ae650b904adafde2405c812a0aac3c6626e89b20c9`.
Other debug tools and `libthread_db` are hashed. Host/kernel/CPU observations are
saved privately; they are not a hardware diagnosis. No downloaded debug symbols
or altered executable was used for the fault classification.

## Causal disposition and next review

| Class | Assessment |
|---|---|
| A — harness/prototype-only | Not established. No concrete harness invalid operation identified. |
| B — shared compiler/Runtime defect | Not established or excluded. No broken production invariant identified. |
| C — LLVM/JIT interaction | LLVM optimization is the failing component, but no causal IR/ABI/optimization violation is established. Location alone does not establish this root-cause class. |
| **D — unresolved** | **Selected.** Native failure captured and phase localized; cause remains pending. |

The smallest defensible explanation is a native general-protection fault while
the original process executes LLVM DSE through VOIDC's shared compilation path.
This capture rules out the current workload's generated logger execution as the
immediate faulting phase; it does not rule out earlier corruption or a defect in
shared compiler/Runtime setup. **Released VOIDC correctness could be affected and
cannot be cleared from this evidence.** Prior passing controls and sanitizer runs
do not change that assessment. Historical LLVM incidents are not assumed causal.

No optimizer workaround, Runtime change, relaxed stack, skipped check, new source
reduction, production fix or logging implementation was made. No focused regression
test can yet encode an identified violated invariant. The exact 61-file workload
and command are retained as an intermittent reproducer; repeated passes would not
prove a future repair.

For Golden Wrench review, the next concrete evidence gap is the omitted instruction
text. A separately authorized bounded unchanged-executable capture could retain
private file-backed executable pages through process-local `coredump_filter`
configuration, with the same stack and original inputs. That would allow resident
code/file comparison; it would not itself prove cause. Further symbol/IR or host
isolation work should follow the resulting evidence rather than a speculative
production change. **No such additional target run was performed here.**

Any later repair needs a cause-specific reduced regression plus the original O2
workload, normal/O2 JIT/AOT checks, the actual hard stack, and applicable sanitizer,
GC/ownership and Runtime checks. Logging's full correctness/performance matrix
remains separately required after crash resolution and authorized implementation.

## Handoff and evidence index

`N/EVIDENCE_INDEX.md` explains the private raw inventory and compact review archive
`N/review/void-log-native-core-review.tar.gz`. The archive contains documentation,
drivers, identity/attempt metadata and selected diagnostic text; no core, executable,
shared library, raw environment or large memory artifact. Raw per-command receipts
and the crash recorder's complete incident remain in private external storage.

This turn adds this document and updates `HANDOFF.md`. Production and repository
test sources remain unchanged. No staging, commits, branches, pushes, hosted CI,
ptrace, root README edit, project-manifest or package-manager work was performed.
The bounded investigation ends here for review; the crash is **not resolved**.
