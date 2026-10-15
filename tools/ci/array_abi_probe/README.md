# Phase 23 array call storage investigation

This standalone LLVM 19.1.7 harness compares raw aggregate calls with explicit
address-based calls. It does not implement production arrays, change their ABI,
enforce a compiler limit or alter either CI workflow.

Full reproduction on native Linux x64, without historical build artifacts:

```sh
python3 tools/ci/array_abi_probe/probe.py \
  --llvm-bin /usr/lib/llvm-19/bin \
  --output build/p23-2-array-calls --cross-targets --jit
python3 tools/ci/array_abi_probe/test_audit.py \
  --llvm-bin /usr/lib/llvm-19/bin \
  --artifacts build/p23-2-array-calls \
  --raw-baseline build/p23-2-array-calls
```

The default `--calling both` includes the original raw experiment unchanged.
`--calling raw` or `--calling address` selects one protocol. `--counts` can select
samples; the full regression checks require the default address counts.
Raw 4096-element aggregate legalization is expensive; each subprocess has a
120-second timeout and every unexpected failure aborts the experiment. Omit
`--cross-targets` for Linux only. Windows x64 and macOS ARM64 measurements are
**compile-only**. This driver intentionally requires native Linux x64 and does
not infer execution from cross-compilation. No compiler project is built and
no dependencies are downloaded. The matching LLVM development SDK, llvm-config,
headers and libraries are required for a single small target-facts helper; this
does not build the VOID compiler.

Raw calls retain LLVM aggregate arguments/results and the original two caller
homes plus a private callee home. Address calls use plain pointer parameters,
an explicit caller-owned result, a separate argument snapshot and a private
callee copy. There are three caller homes and one callee home. The generated
address signature has no `byval`/`sret` attributes or aggregate values crossing
the function boundary. Memcpy intrinsics retain ordinary optimization and may
become target libc calls. The scalar C adapter signature remains unchanged.
Each i32 layout is witnessed by the selected target compiler (size/alignment
4/4, pointer size 8), and the target DataLayout must specify stack alignment 16.
A small LLVM C++ helper also queries actual DataLayout sizes and target
FrameLowering stack alignment/local-area offset. Its DataLayout must agree with
Clang. This is a limited experiment, not a generic layout calculator for the
compiler.

Normal leaves IR unoptimized; O2 uses LLVM `default<O2>`. Both use the current
AOT default machine-code level, `llc -O2`, not machine-code `llc -O0`. PIC is used
for MIR measurements and emitted objects. Independent caller/callee modules and
a separately compiled C host preserve the real boundary without LTO, `noinline`,
volatile, disabled optimization or increased stack reserves. The regression
suite additionally enables ordinary O2 inlining in a combined-module experiment.

The original C assertions remain: every element is checked for two seeds, and
fill/touch/check must each run exactly twice. Address builds then additionally
verify the first argument's snapshot across a later argument's owner mutation,
its independence from callee mutation, evaluation order, later-argument failure,
callee failure after a partial result write, suppression of result consumption
on failure, and bounded trace propagation. The probe uses the existing
message/count/32-frame channel shape. It does not certify source locations,
production diagnostic text, readonly analysis, managed owners, roots or staging.

Linux executes each independent AOT pair with both the inherited stack limit and
hard/soft `RLIMIT_STACK=1048576` before exec. This is a main-process stack limit,
not the original stack probe's mapped worker stack. With `--jit`, Linux also
loads independent IR modules through LLVM lli ORC at generic CPU/machine O2.
All four actual ORC frame warnings must match the corresponding AOT MIR audit.
ORC runs use the inherited process stack limit. They are not production VOID JIT
verification and do not verify the existing host-feature ORC configuration.
No unexplained crash is accepted. Deliberately broken snapshot protocols in the
regression suite must exit exactly 1 through the C assertions, never by a signal.

`audit.py` reconciles planned homes against pinned LLVM post-prologue/epilogue MIR.
It records outgoing areas, saved-register/spill objects, alignment padding and
unmatched homes. Its supported nonoverlapping, fixed-home case computes:

```
P = unrounded preoptimization home inventory, using target size/alignment
R = bytes of nonoverlapping machine objects matched to those planned homes
F = measured machine frame bytes, already including call areas/spills/padding
E = supported target entry-area prefix from FrameLowering, outside F
candidate charge = align_up(P + F - R + E, target stack alignment)
```

For these pinned nonleaf X86/AArch64 profiles, E is minus the selected
FrameLowering local-area offset: 8 on x64, 0 on ARM64. This agrees with the
earlier native frame/stride evidence. It is an entry-slot fact, not a measured
fixed allowance for copies/spills. Other frame shapes still require proof.
The original P is retained even when O2 deletes a home. Frame fields must not be
added again to F. Unsupported incoming areas, unexplained non-spill objects,
opaque/dynamic stack shapes, tail calls or stack realignment receive no budget
verdict. Overlapping planned objects require a separate reuse/lifetime proof.
The tests catch actual hidden raw return homes, omitted homes, faulty plans,
untracked allocations, and an O2 inlining clone. They also show that a preplan
aligned to exactly 65536 can still fail after actual ABI storage is accounted.
No measured residual becomes a fixed allowance. Over-budget samples still execute
as investigations; the harness does not adopt a production resource policy.

Production must use MachineFrameInfo/AllocaInst provenance in the common emitter,
not this MIR text parser or a separate LSP estimator. Red-zone storage, inlining
lineage, arbitrary argument areas, root/staging inventories and borrowed storage
remain proof obligations. See ARRAYS_PROPOSAL.md Sections 12 and 13.

Outputs include `results.json`, `complete.json`, `commands.json`, per-function
`*-audit.json`, generated IR/MIR, native objects and binaries. `complete.json`
marks incomplete runs explicitly. Regression artifacts/logs stay under the chosen
output directory. Frame numbers describe these samples only; they are neither
per-platform constants nor a universal stack-safety guarantee.

## C++ MachineFunction and encoded-object proof

The newer, independent proof uses the same generated address protocol and C host:

```sh
python3 tools/ci/array_abi_probe/machine_proof.py \
  --llvm-bin /usr/lib/llvm-19/bin \
  --output build/p23-2-machine-proof-example --cross-targets
python3 tools/ci/array_abi_probe/test_machine.py \
  --llvm-bin /usr/lib/llvm-19/bin \
  --artifacts build/p23-2-machine-proof-example
```

Use a fresh output directory. This builds only the standalone machine_emit.cpp
against the pinned LLVM development SDK, never the VOID compiler. Source snapshots
and hashes are retained; changing investigation sources during the run fails it.
The driver verifies generated IR and snapshots original AllocaInst identities and
selected-target layouts before IR optimization inside C++. The target profile is
explicit generic CPU, PIC and machine Default/O2 at both normal/O2 IR settings.
This differs from the older llc empty-CPU default; Windows's semantic caller is
280 bytes here, versus 264 in the older profile. Independent `llc -mcpu=generic`
measurements in the regression tests must match all 48 principal/semantic frames.

An actual C++ MachineFunction pass audits after the full TargetPassConfig machine
pipeline, immediately before AsmPrinter. Earlier PEI observations are retained for
comparison. Another C++ pass checks the certificate after AsmPrinter, and LLVM MC
decodes the resulting private object to compare SP writes and call opcodes. Only
x86 ADD/SUB imm32-to-imm8 shortening with identical decoded operands is normalized.
Actual machine-frame mutations, stack-instruction mutations and a Linux encoded
object mutation must all be rejected. Unknown original homes, clones, unsupported
frame shapes, missing incoming ownership and excess budget block object delivery.
Rejected objects remain private memory buffers and are never linked or executed.

The frozen P is never reduced by optimization. R requires the original live
AllocaInst pointer, exact extent/alignment and disjoint machine ranges. Functions
with lifetime markers receive zero subtraction credit; actual slot coloring is
measured, not inferred safe from a surviving name. O2 inlining of the original
address modules and inlining into an originally array-free function both fail for
unproved allocation lineage. An omitted-inventory fixture also fails against the
real machine object. These rejections preserve optimization and the fixed limit.

Windows five-argument ownership is accepted only for the exact supported
void(ptr,ptr,i32,i32,ptr) signature and an already certified actual caller edge:
40-byte setup, a late SP+32 8-byte store, and the callee's matching incoming slot.
Imported local proof JSON must match the delivered caller object hash. The callee
entry SP+40 slot belongs to the caller's 40-byte outgoing area; the callee's own
shadow area stays in its F and its entry return slot is E=8. Wrong shadow-only
proofs, missing stores and stale hashes are rejected. This is a narrow investigation
certificate, not an untrusted-input attestation format or a general ABI classifier.

Accepted Linux objects execute against all original assertions via independent
AOT linking (inherited and 1 MiB RLIMIT_STACK) and standalone ORC object loading
(1 MiB). The ORC host adapter is compiled separately; this does not certify VOID's
IR compilation callback, GC, publication/reload path or current host-feature JIT.
Windows x64 and macOS ARM64 are cross-compiled/decoded only. No native execution
on those systems, full suite, hosted CI or production policy change is implied.

Artifacts include `complete.json`, `results.json`, `controls.json`, `commands.json`,
source snapshots/hashes, input and optimized IR, C++ frame certificates, per-object
SHA256, disassembly, separately linked binaries, and isolated deliberate-failure
reports with no corresponding delivery object. Tests add independent llc objects
under `.independent.o`; those are measurement witnesses, not certified deliveries.
See ARRAYS_PROPOSAL.md Section 14 for measurements, exact scope and remaining
GC/staging, transformation-lineage, target/profile and const-borrow obligations.

To independently identify post-PEI changes in the Linux normal semantic caller:

```sh
/usr/lib/llvm-19/bin/llc -mcpu=generic -O2 -relocation-model=pic -filetype=obj \
  -print-after-all -filter-print-funcs=snapshot_case \
  build/p23-2-machine-proof-example/x86_64-pc-linux-gnu/normal/semantics-caller.optimized.ll \
  -o build/p23-2-machine-proof-example/late-pass-witness.o \
  2> build/p23-2-machine-proof-example/late-pass-trace.log
```

The Control Flow Optimizer (`branch-folder`) merges three epilogues into one after
PEI. The C++ `after_pei`/late snapshots show 20 versus 10 SP-writing instructions
with F=232 unchanged. This trace is a measurement witness, not a certified delivery.
