# G51 — Avoid rescanning UTF-8 concatenation operands

Accepted production baseline G48 `4790a7a52ecf42732d12e3adc35cac3812246516`. G46, G49 and G50 remain separate held implementations. This worktree carries the verified G49 report and the G50 report with its final recovery. The source comes from G48 plus the changes described here. [G49 report](G49_DICTIONARY_INSERTION_PROBE_REUSE.md) · [G50 final recovery](G50_JSON_TEXT_CONSTRUCTION.md#final-recovery).

## Established implementations

Rust's released `String::push_str` treats a `str` as already validated UTF-8 and copies its bytes without another decoding pass. Its slice join also checks the output extent and writes the existing slices into one allocation. Go's runtime concatenation and Odin's join size and copy byte strings using their existing allocation machinery. The applicable technique for VOID is relying on UTF-8 validity already proven at managed string admission. VOID keeps its final assembled-payload check and its representation. [Rust 1.91 String](https://github.com/rust-lang/rust/blob/1.91.0/library/alloc/src/string.rs#L1084) · [Rust slice join](https://github.com/rust-lang/rust/blob/1.91.0/library/alloc/src/str.rs) · [Go 1.25 concatenation](https://github.com/golang/go/blob/go1.25.0/src/runtime/string.go#L22) · [Odin strings](https://github.com/odin-lang/Odin/blob/master/core/strings/strings.odin#L590).

## Exact production change

The shared private `Runtime::string_parts_into` takes a flag for its existing operand scans. `Runtime::string_into` retains both scans on its original raw-input path. `Runtime::concat_into` skips them after the two existing calls to `string_bytes`. `construct_into` still validates the complete assembled payload before allocation and publication. There is one input preparation path and one construction path.

The current Heap checks object identity, intrinsic string kind and stored length before exposing a managed-only immutable view. The Runtime's public construction path validates the payload before admitting a string, and its general allocation method rejects intrinsic string allocation. The normal string representation is immutable and precise roots keep both original inputs live. The empty string contributes an empty valid sequence. Joining two complete UTF-8 sequences preserves validity at the seam. The Runtime copies the views into the existing owned payload before its first allocation checkpoint or safepoint. Neither input borrow crosses that boundary. The output remains in its old registered root until the same complete replacement is ready.

Managed calls evaluate their operands and reach the same Runtime entry in the same order. The Runtime retains the left then right Heap checks, output-root checks, representable-extent check, empty result rule, staging allocation, final UTF-8 validation, allocation checkpoint, precise publication and all copies. On a supported managed input the removed scans cannot reject. Arbitrary native bytes still validate before any staging allocation. No held decode, dictionary or JSON implementation is included. `text.join` retains its byte-encoding and append path. It serves as a control.

## Fault and ownership controls

The maintained native string proof adds an allocation sweep with output aliased to the left operand, forced collection, exact NUL and Unicode output, immutable input checks and rejection of an ordinary object offered as text. It is exercised by the native source suite and independently emitted and linked normal and optimized AOT string suites. Existing source coverage checks evaluation order, string limits, UTF-8 diagnostics, initialization, reload and cleanup.

The independent G48/G51 native fault replay uses the existing managed-test allocation hook. Every result matched across 24 injected budgets for valid aliased concatenation and 24 for malformed raw UTF-8. Valid concat reports allocation failure at budgets 0–8 and success at 9–23. Malformed raw input reports allocation failure at budget 0 while materializing its diagnostic and reports invalid UTF-8 at 1–23. Failure preserves the aliased output and both rooted inputs. Valid operations succeed on retry after collection.

## Measured work reduction

Sixty independently linked executions validated outputs and counted scanner calls and admitted bytes inside the actual Runtime bodies. A separate observer build used fixed timing text so decimal elapsed-time reporting could not change the unrelated string-length totals. The initial observer run and its failed comparison are preserved, along with the linker retry that restored the allocation wrapping options. Throughput uses the ordinary fixture and libraries without observers.

Counts below include one warmup and three tested actions. A separate byte oracle computes the expected traversal over valid input, including the growing prefixes in the chain. Normal and optimized AOT counts agree.

| Workload | Concats | Redundant operand scans G48 → G51 | Operand bytes rescanned G48 → G51 | Native C++ allocation requests on each side |
| --- | ---: | ---: | ---: | ---: |
| concat-short | 32,768 | 65,536 → 0 | 753,664 → 0 | 44,826 |
| concat-1024 | 8,192 | 16,384 → 0 | 8,388,608 → 0 | 36,198 |
| concat-long | 384 | 768 → 0 | 25,165,824 → 0 | 18,462 |
| concat-unicode | 1,536 | 3,072 → 0 | 5,652,480 → 0 | 15,342 |
| concat-chain | 9,216 | 18,432 → 0 | 3,128,832 → 0 | 30,822 |

Both sides publish the same number of complete, validated results. Every measured output validation, output byte count, raw input validation and C++ allocation request matches. The long case eliminates 25,165,824 bytes of scanning while retaining the same number of output bytes scanned. The repeated join, sorting, arrays, dictionaries and integer controls call the selected concat path zero times. The JSON cases call it only during fixture setup. Their independent JSON output byte oracles match G48.

The independently emitted application objects match byte for byte in each AOT mode. The baseline Runtime archive substitutes just the G48 `coordination.cpp` object compiled with the same flags and its original private declaration. All other members match. The held `stdlib` changes never enter either image. Disassembly records the new private flag in the string constructor and linked symbol placements separately from the application instructions.

## Qualification

Linux Release and fatal ASan/UBSan/LeakSanitizer each passed 32/32, including the string source proof and both independently linked AOT string suites. Fatal TSan passed three Runtime controls and 90 checked benchmark samples from normal and optimized AOT linked with the TSan Runtime. Its separate concurrent native proof exercised 64 forced collections, cooperative worker parking, two workers concatenating shared immutable inputs, exact rejection of a foreign Heap string, and cleanup to the two rooted inputs. The Linux lanes use a hard 1 MiB stack.

Native Windows 10 x64 build 19045 passed 26/26 each in Debug and Release. Both profiles passed the separately checked string fixture in normal/O2 JIT and independently linked normal/optimized AOT, for 360 verified sample answers. The 3,319 compile-input hashes, compiler and Runtime digests, target selection, and checked installed-provider hashes are retained. LLVM/Clang/LLD use 23.1.4 revision `7a941dfb2877a0b8cf30323fefa85ad019e0b0f5`. Compilers, selected tests and separately linked fixtures reserve 1 MiB in the PE headers.

## Linux throughput

The exact matched G48/G51 executables ran on one AMD Ryzen 7 1700X host pinned to logical CPU 3 under a hard 1 MiB stack limit. Builds and observation finished before the throughput run. The source fixture performs one warmup and three measured actions per case in each process. Normal and O2 JIT run with the disk cache disabled. Normal and optimized AOT are separately emitted and linked. Eight blocks rotate mode order and alternate G48/G51 process order. An independent byte and value oracle checked every one of the 2,880 timed answers. The table reports the median of eight paired elapsed-time changes using the median of the three timed actions in each process. Positive means the G51 action took longer. Full times, individual pairs, executable hashes, host state and replay are retained.

| Workload | Normal JIT | O2 JIT | Normal AOT | Optimized AOT |
| --- | ---: | ---: | ---: | ---: |
| concat-short | -9.16% | -16.01% | -3.37% | -7.43% |
| concat-1024 | -42.84% | -42.75% | -29.86% | -32.56% |
| concat-long | -56.15% | -56.14% | -41.12% | -40.01% |
| concat-unicode | -45.13% | -44.09% | -44.21% | -44.81% |
| concat-chain | -38.21% | -33.66% | -36.30% | -38.31% |
| join-short | +1.83% | +0.42% | +1.98% | -0.80% |
| join-1024 | -0.70% | -0.04% | -0.64% | -1.60% |
| nested-96 | -2.67% | +0.92% | -2.04% | -0.49% |
| typed-128 | +0.20% | +5.88% | +0.32% | -1.40% |
| dictionary-1024 | -0.31% | +4.07% | +0.14% | +0.03% |
| sort-1024 | -1.60% | +2.59% | +0.18% | +0.91% |
| integer-1000000 | -0.23% | -0.14% | +0.46% | -0.40% |
| integer-4000000 | -1.54% | +0.57% | -0.32% | +1.37% |
| array-1024 | -0.49% | +0.04% | +0.60% | +2.62% |
| array-4096 | -3.91% | -0.58% | +0.94% | +2.31% |

All 128 paired measurements of 1 KiB, 64 KiB, Unicode/escape-heavy, and chained concatenation favor G51. Short concatenation improves in 29 of 32 pairs. The 64 KiB case has paired elapsed reductions of 40.01–56.15% in every mode. The output still incurs one complete validation scan, byte copying and the original allocation requests. The improvement measures a narrower text operation. It does not extend to join or JSON serialization as a whole.

The mixed G51 image changes private code generation around raw string construction. On this x86-64 Linux build, `string_into` grows from an 11-byte tail jump into a 21-byte wrapper with a call and stack adjustment for the new flag. The shared helper grows from 1,507 to 1,543 bytes, and the method for concat grows from 191 to 204 bytes. The final constructor retains its 1,668-byte size. Both linked AOT modes place `vd_string_concat`, `vd_gc_poll` and `vd_worker_checkpoint` 64 bytes later in G51. Selected array list read and push symbol addresses are unchanged. These are observations from map/disassembly comparison. They cannot identify the cause of a timing change by themselves. [LLVM sibling calls](https://llvm.org/docs/CodeGenerator.html#sibling-call-optimization) · [Pinned X86 call lowering](https://raw.githubusercontent.com/llvm/llvm-project/7a941dfb2877a0b8cf30323fefa85ad019e0b0f5/llvm/lib/Target/X86/X86ISelLoweringCall.cpp).

## Optimized AOT regression checks

The isolated replay used the exact linked images and identical workload inputs on the same core. The table gives process-median elapsed milliseconds for each build, then the median and full range of eight paired percentage differences. Each replay alternates process order and uses the original output oracle. The mixed run at the top exercised all cases in each process. The isolated run launched just the selected case.

| Workload | Context | G48 ms | G51 ms | Paired delta | Paired low to high | G51 slower |
| --- | --- | ---: | ---: | ---: | ---: | ---: |
| integer-4000000 | Mixed | 56.823 | 57.207 | +1.37% | -14.23% to +24.70% | 5/8 |
| integer-4000000 | Isolated | 57.383 | 57.438 | +0.92% | -2.02% to +3.14% | 5/8 |
| array-1024 | Mixed | 0.951 | 0.973 | +2.62% | -5.55% to +26.97% | 6/8 |
| array-1024 | Isolated | 0.831 | 0.849 | +1.90% | -5.32% to +24.59% | 5/8 |
| array-4096 | Mixed | 3.736 | 3.841 | +2.31% | -6.80% to +7.06% | 5/8 |
| array-4096 | Isolated | 3.374 | 3.409 | -0.22% | -6.92% to +17.87% | 4/8 |

The other focused replays give normal-AOT short join −2.00% after +1.98%. O2 JIT typed JSON moves from +5.88% to +1.04%, dictionary from +4.07% to −0.07%, and sorting from +2.59% to +1.40%. O2 JIT array-4,096 moves from −0.58% to +3.76%, slower in seven of eight replay pairs. Both outcomes remain in the record. Optimized AOT array-1,024 gives small positive elapsed medians in all runs and wide paired ranges. The larger individual outliers fail to provide a repeatable effect of comparable size.

### 1. Executable layout and alignment

The pinned LLD relink keeps the optimized AOT integer hot loop at `0x325c80` and the two array backward-loop targets at `0x325a60` and `0x325b2e`. Their loop instruction addresses, branch sites, targets, and positions within 64-byte lines are unchanged. The output ELF section ordering remains the same. `.text` starts at the same address and grows by 64 bytes. `.rodata` start and size stay fixed. `.plt` and `.got` move 64 bytes. The GC poll moves from `0x3e7060` to `0x3e70a0` and the worker checkpoint moves from `0x3f5690` to `0x3f56d0`. Both retain their offsets within 64-byte lines. The normal AOT comparison shows the same 64-byte downstream shift and unchanged local loop placement. The exact binaries, ELF metadata, branch tables and disassembly are retained.

The layout movement is real. Its cost is unproven. The eight matched AMD fetch-counter pairs give L2 instruction fills of −1.36% for integer-4M, −7.01% for array-1,024 and +17.26% for array-4,096. The array-4,096 fill changes range from −23.21% to +61.88%, while elapsed changes in the same run have a −1.21% median. The corresponding fetch-stall medians are +0.68%, +3.53% and +0.43%, with individual pairs on both sides of zero. The data rule out a change in the alignment of the inspected loops and fast paths. They leave open possible cache-index or predictor-address sensitivity.

### 2. GC polling and worker checkpoints

The isolated G48/G51 observer instruments the actual Runtime entry, cold poll rendezvous, worker checkpoint, active-worker branch, and Heap collector bodies. Instrumented times are excluded from throughput. Counts include startup, one warmup and three timed actions, and match exactly:

| Workload | GC polls in each | Worker checkpoints in each | Cold rendezvous or active-worker checks | Heap collections in each |
| --- | ---: | ---: | ---: | ---: |
| Integer 1M | 16,000,305 | 16,000,305 | 0 | 0 |
| Integer 4M | 64,000,305 | 64,000,305 | 0 | 0 |
| Array 1,024 | 33,081 | 33,081 | 0 | 0 |
| Array 4,096 | 131,385 | 131,385 | 0 | 2 |

Both builds emit the same calls in each loop. The poll still checks managed TLS context and the acquire-loaded request, and returns on the idle path with the same stack and register operations. The worker checkpoint still reads worker TLS and returns via the no-worker path. The source, native archive members and cold path instructions are retained. Sampling cycles at 2,999 Hz on eight alternating integer-4M pairs assigns a median +1.69% cycle weight to the poll, with four pairs higher and four lower (−7.00% to +19.53%). Worker attribution is much noisier and sampling skid limits per-function estimates. Whole-process count data are more stable. The controlled run establishes equal required checks and no added collector or park work. It provides no reproducible increase in idle check cost.

### 3. LLVM-generated Runtime instructions

The direct comparisons retain the same integer and array instruction sequences, spill slots, memory-access forms and branch conditions. The linked diff contains relocation displacements, including the calls to the shifted poll and checkpoint. The compiled Runtime members for ABI polling, async checkpoints, arrays and the Heap are byte-identical. A linked comparison normalizes address-dependent operands and separately records raw machine bytes. The selected checked list operations, array insertion, poll, checkpoint, rendezvous and Heap collector retain their instruction sequences. The private text wrapper and concat bodies do have native instruction changes. Concat is absent from the timed integer and array actions. The raw string wrapper adds a call and stack adjustment, with no consistent short-join elapsed penalty in its own replay.

A separated PMU replay on the exact normal-AOT long concat shows −46.44% retired user instructions, −47.20% branches and −35.81% cycles, with −39.41% timed elapsed. Integer-4M optimized AOT retires effectively the same instructions and branches, with +0.33% cycles, +0.32% branch misses and +0.13% elapsed. Array-1,024 shows effectively unchanged instructions, +0.25% branches, +1.51% branch misses, −0.13% cycles and +1.38% elapsed. Branch misses for that array rise in seven of eight pairs. That mild signal does not explain a persistent cycle cost in this run. All counts cover whole isolated processes and all data are preserved independently of the timing-only runs.

## Disposition

**Recommend integration after source review on G48.** AOT and JIT show a repeatable drop in actual concatenation work. The three targeted checks show a 64-byte linked shift, preserved loop alignment, equal GC activity and no added instructions in the relevant hot paths. The substantial unrelated slowdown does not reliably reproduce on this host. Retain the small array-1,024 optimized-AOT medians, the branch-miss signal, and the variable O2 JIT array and sorting measurements. Linux provides the throughput evidence on one physical CPU. Windows provides correctness qualification for this source.

The new private flag adds one check per construction and makes operand validity explicit at the two callers. It adds no allocation, storage, or public interface. No further source correction has a reproducible target in the existing evidence. Leave the qualified source untouched. Preserve exact G49 and G50 held source and archives, the full history, all adverse measurements and prior Wrench guidance. The retained archive includes source postimages, an applicable patch, input oracles, timing and PMU records, and the Linux and Windows qualification receipts.
