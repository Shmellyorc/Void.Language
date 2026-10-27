# Compiler-health benchmark record

The frozen baseline measurements below are preserved. Current before/after,
profile, cache and application tables are in the
[stabilization measurements](stabilization/BENCHMARKS.md); see the
[review handoff](stabilization/HANDOFF.md) and [audit](stabilization/AUDIT.md).

27 October 2026. Baseline **`1d4ad15f5215838d209e48bb47ac7c3f52b05371`**.
[Interpretation](REPORT.md), [inventory](ISSUES.md), [repair plan](ROADMAP.md),
[evidence and extraction](evidence/README.md).

## Method and provenance

- Linux 7.2.11 PikaOS x86_64 / glibc 2.44; AMD Ryzen 7 1700X, 8 cores / 16 threads.
  Existing Release build (`-O3 -DNDEBUG`), pinned LLVM **19.1.7**.
- Unchanged CLI SHA-256:
  `3342a2153fc909a98111207db82a8c7b2736983a5311e33a4f68369ec391a952`.
  Manifest source/compiler hashes are checked again at the end. Helpers have
  separate source/library/binary provenance.
- Every measured child has a real **1,048,576-byte soft and hard stack limit**.
  ASLR remains **2**. No ptrace, debugger, perf attachment, sanitizer or CTest run.
- Fresh processes run serially. `wait4` records child user/system CPU, peak RSS,
  faults and context switches; raw records also contain exact commands, cwd,
  load average, source/compiler hashes and output paths. Native-build CPU can
  exceed wall time through child concurrency; peak RSS is the Linux process/
  waited-child high-water value, not summed simultaneous memory of a process tree.
- Host is not CPU-isolated; boost is enabled and load averages are retained.
  No OS page-cache flush or controlled disk-cold condition. “Cold cache” means
  empty **this scratch fixture's** `.void` object cache; warm runs verify `hit`.
- Three samples for normal startup/check/cache-hit groups, five for native runs;
  single cold-cache/build and expensive width/phase/llc observations. Ranges
  describe actual variation, not confidence intervals. Do not turn local seconds
  into universal hardware/platform guarantees.
- CLI output is asserted, including DTO roundtrip equality. The repository's
  existing `tests/text_encoding/crash.py` preserves exact `_crash.void` artifacts
  for abnormal exits/internal IR errors/timeouts. Bound: 480 seconds per costly
  CLI/llc child, smaller limits for smaller probes; no widening after a timeout.

### Workloads

| Fixture | Content / expected stdout |
|---|---|
| tiny | `print(7)`; `7` |
| gameplay | Small game-like math/value/list program using sqrt and clamp on (3,4)/(5,12); `18`. A small proxy, not a full game benchmark. |
| DTO N | Managed DTO with N public i32 fields, parse exact ordered object with `json.parse_as`, stringify equality; N-1 then `true`. N=8,16,32,64,128. |
| plain N | Same managed field width, ordinary named construction and last-field output, without JSON. |
| unused import | Tiny print plus only `use json as json` or `use text as text`; `7`. |

These are new fixed research fixtures. The historical 246.61-second wide O0 and
148.76-second shapes-O2 records are preserved in JSON evidence, but differ in
workload/build history. They are evidence of unacceptable latency, not a valid
controlled speedup comparison to this table.

## Unchanged CLI: startup and execution

All times seconds. CPU is median user+system; RSS is maximum observed MiB.
O0 below means current `--no-opt`: **IR O2 disabled, codegen still LLVM Default**.
`--cache` explicitly selects that unoptimized IR mode. `check-dir` exercises
shared directory/main discovery. Native executables are built independently by
`voidc build`; native run time excludes compilation. AOT uses the existing generic
CPU target, while JIT detects host CPU/features, so AOT build versus JIT compile
is not a same-target LLVM phase comparison.

| Fixture | Mode | n | Median wall | Wall range | CPU | Peak RSS MiB |
|---|---|---|---|---|---|---|
| tiny | check | 3 | 0.007199 | 0.006634–0.009363 | 0.006155 | 30.5 |
| tiny | check-dir | 3 | 0.008389 | 0.007111–0.009414 | 0.007540 | 30.6 |
| tiny | O0 | 3 | 0.023201 | 0.019231–0.024753 | 0.022005 | 43.0 |
| tiny | O2 | 3 | 0.022495 | 0.021656–0.024313 | 0.021370 | 44.4 |
| tiny | cold-cache | 1 | 0.024414 | 0.024414–0.024414 | 0.023215 | 42.9 |
| tiny | warm-cache | 3 | 0.010906 | 0.009246–0.011658 | 0.009846 | 33.5 |
| tiny | build O2 | 1 | 0.097614 | 0.097614–0.097614 | 0.102688 | 64.6 |
| tiny | native run | 5 | 0.004298 | 0.003680–0.004582 | 0.003410 | 12.5 |
| gameplay | check | 3 | 0.017661 | 0.013549–0.017810 | 0.016602 | 33.7 |
| gameplay | check-dir | 3 | 0.016745 | 0.014843–0.018125 | 0.015686 | 34.2 |
| gameplay | O0 | 3 | 0.058437 | 0.058201–0.058874 | 0.056918 | 47.9 |
| gameplay | O2 | 3 | 0.071755 | 0.071506–0.077065 | 0.070235 | 50.3 |
| gameplay | cold-cache | 1 | 0.055471 | 0.055471–0.055471 | 0.054076 | 47.9 |
| gameplay | warm-cache | 3 | 0.019611 | 0.016022–0.021134 | 0.018501 | 37.8 |
| gameplay | build O2 | 1 | 0.140957 | 0.140957–0.140957 | 0.149648 | 65.2 |
| gameplay | native run | 5 | 0.003839 | 0.003469–0.005007 | 0.002826 | 12.5 |
| dto8 | check | 3 | 0.102103 | 0.102081–0.148692 | 0.100484 | 44.8 |
| dto8 | check-dir | 3 | 0.104160 | 0.101913–0.116909 | 0.102402 | 44.5 |
| dto8 | O0 | 3 | 14.623124 | 14.399893–14.847745 | 14.489441 | 218.3 |
| dto8 | O2 | 3 | 24.977001 | 24.694993–26.847279 | 24.759375 | 236.9 |
| dto8 | cold-cache | 1 | 15.974983 | 15.974983–15.974983 | 15.819232 | 216.9 |
| dto8 | warm-cache | 3 | 0.126823 | 0.126076–0.137662 | 0.124881 | 53.8 |
| dto8 | build O2 | 1 | 13.006568 | 13.006568–13.006568 | 12.914208 | 213.5 |
| dto8 | native run | 5 | 0.010768 | 0.010542–0.013090 | 0.009809 | 12.6 |

Invalid CLI startup/usage control took 0.006431 s with expected status 2; it is
only a process/loader/argument-error floor, not an isolated valid-program startup
phase. Directory discovery adds no demonstrated seconds-scale delay in these
small graphs. DTO8 cold compilation is seconds while native/first invocation is
milliseconds; the observed slow launch is predominantly compilation.

## DTO width ladder and plain controls

For width8 use the three-sample medians above. Widths16–128 are one sample each.
CPU/RSS are retained per operation in JSONL; this table shows O0/O2 CPU and O2 RSS.

| Fields | Check wall | O0 wall | O0 CPU | O2 wall | O2 CPU | O2 RSS MiB | Plain O2 wall |
|---|---|---|---|---|---|---|---|
| 8 | 0.1021 | 14.623 | 14.489 | 24.977 | 24.759 | 236.9 | 0.0270 |
| 16 | 0.1171 | 21.772 | 21.551 | 40.779 | 40.406 | 373.9 | 0.0278 |
| 32 | 0.1382 | 34.819 | 34.407 | 63.749 | 63.072 | 582.7 | 0.0307 |
| 64 | 0.1851 | 64.397 | 63.680 | 124.512 | 123.277 | 1037.1 | 0.0391 |
| 128 | 0.2898 | 136.479 | 134.889 | 309.755 | 306.891 | 1868.2 | 0.0579 |

The width128 O0 run uses about 1.81 GiB peak RSS. Width controls show that wide
ordinary struct construction alone does not explain the DTO compilation cost.
The expensive decoder shape, common imported code and backend processing matter.
No timeout increase is counted as a repair.

## Phase boundaries: scratch API and dump helpers

Helpers include the unchanged production backend in an **O1 scratch translation
unit**, linked to existing Release core/Runtime and pinned LLVM libraries. Public
API helper uses that backend object. These timings corroborate the CLI; they
are not exact same-binary subtraction or production instrumentation. Text dump
and inventory time is measured separately, not charged to LLVM optimization.

Dump-helper seconds, one sample each:

| Fixture | Analysis | Typed lower + validation | Extra validation | LLVM lower + validation | Verify/O2/verify |
|---|---|---|---|---|---|
| tiny | 0.000628 | 0.000147 | 0.000044 | 0.000350 | 0.004426 |
| gameplay | 0.006430 | 0.001260 | 0.000896 | 0.002023 | 0.019013 |
| dto8 | 0.093944 | 0.193476 | 0.181133 | 0.258567 | 3.939201 |
| dto16 | 0.103975 | 0.251235 | 0.235197 | 0.317797 | 4.757202 |
| dto32 | 0.128957 | 0.402918 | 0.393719 | 0.480956 | 7.998961 |
| dto64 | 0.173579 | 0.955139 | 0.933622 | 1.047165 | 22.887237 |
| dto128 | 0.264482 | 2.993684 | 2.960963 | 3.146758 | 106.593065 |

Analysis combines source loading, lex/parse, checking and generic specialization;
this audit does **not** independently time those internal subphases. Typed lowering
also includes validation. The extra validation is a diagnostic probe, not an
additional cost to add blindly to unchanged CLI totals. Host target setup is
~0.3–0.4 ms; explicit-file discovery ~0.02 ms in the dump helper.

Actual public API helper boundaries (seconds):

| Fixture | Mode | Jit setup | Aggregate compile | First invoke | Second invoke | Explicit teardown |
|---|---|---|---|---|---|---|
| tiny | O0 | 0.000980 | 0.011494 | 0.000094 | 0.000014 | 0.000487 |
| tiny | O2 | 0.000916 | 0.013255 | 0.000100 | 0.000017 | 0.000319 |
| gameplay | O0 | 0.000914 | 0.041156 | 0.000127 | 0.000028 | 0.000775 |
| gameplay | O2 | 0.000982 | 0.057143 | 0.000117 | 0.000027 | 0.001049 |
| dto8 | O0 | 0.000747 | 15.904866 | 0.006737 | 0.006085 | 0.005430 |
| dto8 | O2 | 0.000815 | 28.754364 | 0.006645 | 0.005971 | 0.004955 |

Aggregate compile includes public validation, lower/IR optimization, native
machine generation, ORC materialization/linking/lookup and metadata admission.
ORC-only linking and parsing-versus-specialization are **not independently
isolated** here. JIT setup is ~1 ms and exact-IR external codegen alone explains
most DTO aggregate time; neither observation proves ORC has zero cost. First
invoke includes ordinary Runtime initialization/global admission/program output;
teardown sums explicit code/Jit/typed/analysis resets, not process loader costs.

## IR structure and root pressure

Counts are definitions, not declarations. LLVM instruction counts include the
whole emitted module. `root_slots` comes from constant arguments to generated
frame-enter calls; it is registration width, not peak live heap. The worst DTO
root function is the generated **decoder `$json$0`**, not the wide construction
closure. Maximum block below means LLVM IR instructions in a block, not machine
instruction count. Static alloca bytes are summed IR allocations, not final
machine stack-frame size.

| Fixture | Functions pre | Instructions pre | Max block pre | Max block post | Worst roots | Worst alloca bytes pre | Worst alloca bytes post | IR text MiB pre/post |
|---|---|---|---|---|---|---|---|---|
| tiny | 2 | 190 | 81 | 96 | 9 | 412 | 408 | 0.01 / 0.01 |
| gameplay | 8 | 961 | 210 | 199 | 19 | 1049 | 824 | 0.07 / 0.06 |
| dto8 | 73 | 100124 | 7277 | 9129 | 865 | 38228 | 37096 | 8.66 / 6.09 |
| dto16 | 73 | 113260 | 12437 | 15649 | 1481 | 65404 | 63560 | 9.80 / 6.93 |
| dto32 | 76 | 140880 | 21558 | 27515 | 2595 | 114324 | 111696 | 12.17 / 8.70 |
| dto64 | 76 | 194512 | 40918 | 52315 | 4931 | 217140 | 212304 | 16.81 / 12.18 |
| dto128 | 76 | 301776 | 79638 | 101915 | 9603 | 422772 | 413520 | 26.13 / 19.19 |
| import-json | 64 | 73505 | 4547 | 5880 | 536 | 25435 | 24680 | 6.39 / 2.77 |
| import-text | 38 | 18633 | 806 | 1015 | 95 | 4287 | 4152 | 1.47 / 1.20 |

Post-O2 DTO128 still has 65 functions and 284,226 instructions. In this coordinated synchronous decoder, each reference instruction's root home
and the complete slot descriptor array are initialized in the entry path. The generator emits repeated fallible/reference-heavy field
branches. The combination explains a source-supported mechanism for the measured
large scheduling regions; a changed-generator experiment is still required to
establish its precise benefit. The existing >=32-field construction closure
reduces one region but leaves the dominant decoder root frame intact.

## Controlled LLVM 19.1.7 machine-code experiments

Each pair uses the **identical dumped pre-O2 IR**, host CPU/features from
`*.target.txt`, PIC relocation and `llc -O=0` or `-O=2 -filetype=obj -time-passes`.
Post rows instead use the helper's O2-optimized IR. This isolates backend pass
policy from frontend work. It does not establish that every pass difference is
scheduling, nor make a modified CLI timing claim.

| Fixture | IR input | Codegen | Wall s | CPU s | RSS MiB | Object bytes |
|---|---|---|---|---|---|---|
| dto8 | pre | O0 | 2.120 | 1.526 | 126.8 | 2604088 |
| dto8 | pre | O2 | 15.539 | 15.324 | 213.7 | 1893616 |
| dto8 | post | O2 | 23.664 | 23.407 | 224.8 | 1276944 |
| dto32 | pre | O0 | 2.272 | 2.253 | 160.5 | 3641352 |
| dto32 | pre | O2 | 36.204 | 35.787 | 503.2 | 2650880 |
| dto32 | post | O2 | 52.804 | 52.254 | 556.9 | 1644744 |
| dto128 | pre | O0 | 5.862 | 5.814 | 320.3 | 7728640 |
| dto128 | pre | O2 | 123.463 | 122.098 | 1826.6 | 5630384 |
| dto128 | post | O2 | 195.309 | 192.988 | 1752.2 | 3093200 |

Machine pass receipt highlights (pass wall percentages, not process percentages):

| Input | Post-RA top-down scheduling | Machine instruction scheduler | Other notable cost |
|---|---|---|---|
| DTO8 pre / cgO2 | 7.603 s / 50.1% | 4.116 s / 27.1% | X86 instruction selection 11.6%; greedy RA ~0.9% |
| DTO8 post / cgO2 | 14.797 s / 63.3% | 4.254 s / 18.2% | X86 selection 12.8%; greedy RA ~0.4% |
| DTO128 post / cgO2 | 135.644 s / 69.8% | 32.847 s / 16.9% | Selection 5.5%; coalescing 3.4% |

Thus the two schedulers account for ~86.7% of the measured post-O2 DTO128
machine pass time. **Register allocation is not dominant.** This is not proof of
an LLVM bug. Lower codegen changes several passes and generated-code quality;
only an explicit later policy decision may change `--no-opt`. Default O2 is
unchanged. See pinned-source references and risks in [REPORT](REPORT.md).

A separate same-IR DTO32 `opt '-passes=default<O2>' -time-passes -disable-output`
run took 8.443 s wall / 8.206 CPU / 127.6 MiB RSS. Pass wall shares: InstCombine
29.1%, MemCpyOpt 22.3%, DSE 20.8%, SROA 3.9%. IR optimization is substantial and
its cost rises sharply with width. The DSE timing is unrelated evidence from the
historical DSE fault; no causal connection is established.

## Unused source import controls

| Fixture | Mode | Wall s | CPU s | RSS MiB |
|---|---|---|---|---|
| import-json | check | 0.091575 | 0.089800 | 41.6 |
| import-json | O0 | 8.615722 | 8.534990 | 149.8 |
| import-json | O2 | 10.829944 | 10.719863 | 170.6 |
| import-text | check | 0.029815 | 0.028396 | 33.3 |
| import-text | O0 | 0.853665 | 0.847212 | 58.5 |
| import-text | O2 | 1.700970 | 1.689325 | 62.7 |

These are aggregate costs of unused imported source, not isolated native-link
latency. They confirm a general compilation cost independent of DTO generation.
Existing graph checking and broad function emission are current architecture; later
CLI emission reachability must retain diagnostics, globals, dispatch, metadata,
callbacks, native/export and reload contracts, including existing intentional
deferred checking of unused generic-template bodies.

## Runtime-only root-width experiment

One sweep, 5,000 operations/row, real registered slots and one valid i32 list
count7; other roots null. First and last slot placement, new token for each
operation versus one reused **still-live** token. Setup/allocation measured
separately; no GC, no LLVM. Every query checksum is 35,000.

Nanoseconds per operation, computed from raw CSV wall time:

| Roots | Last fresh | Last reused | First fresh | First reused |
|---|---|---|---|---|
| 1 | 254.4 | 188.5 | 224.0 | 184.8 |
| 64 | 494.3 | 404.1 | 248.5 | 209.6 |
| 512 | 2002.3 | 1497.7 | 228.8 | 188.0 |
| 4096 | 14791.4 | 10245.9 | 217.9 | 221.3 |
| 16384 | 82879.1 | 81884.3 | 300.8 | 187.6 |

This confirms position-sensitive scanning rather than allocation/GC alone.
Reusing a token does not remove current membership revalidation. Do not claim
this is a full JSON profile or an estimate of frame registration/collection
cost. It motivates precise checked lookup/root-width engineering after smaller
code-generation improvements. Existing `tests/runtime/root_lookup_measure.cpp`
can host later before/after measurements; no new benchmark framework is needed.

## Independent frontend checks and arithmetic scaling

Inference receipts contain **24 `voidc check` invocations**: 17 successes and
7 expected reproduced source errors, no abnormal exit. Propagation direct/list/
dictionary/double-result, existing-task await, direct async-call argument and
choice inference fail; explicit/local controls and immediate-call await pass.
The currently passing immediate await depends on two wrong hints cancelling.
Exact sources and diagnostics are in `inference.tar.gz`.

Single bounded observations (seconds), not repeated statistical medians:

| Shape | Operators | Wall |
|---|---|---|
| Left generic argument | 4 | 0.00661 |
| Left generic argument | 8 | 0.00708 |
| Left generic argument | 12 | 0.01398 |
| Left generic argument | 16 | 0.10508 |
| Left generic argument | 18 | 0.38752 |
| Left generic argument | 20 | 1.51388 |
| Balanced generic argument | 20 | 0.00687 |
| Nongeneric ordinary return, left chain | 20 | 0.53939 |
| Extracted `+=` then generic argument | 20 | 0.00860 |

The source recursion and shape controls establish exponential duplicated hint
work before LLVM. [INFERENCE_REPAIR](INFERENCE_REPAIR.md) specifies the minimal
noncached traversal correction and semantic regressions.

## Reproduction and interpretation limits

Extract evidence archives into a fresh scratch directory; retain the originals.
`benchmark/bench.py`, `phases.py`, `remaining.py` and exact fixtures record the
executed workload/driver, with commands in JSONL. Paths in helper build scripts
and phase-driver constants describe this machine. Adjust paths **in scratch
copies only** for another checkout; do not overwrite baseline evidence or run
against a changing build. Rebuild the scratch helpers with the same pinned
libraries/profile if needed. No preserved binary is silently substituted.

The equivalent bounded CLI operations are:

```text
voidc check <fixture/main.void>
voidc check <fixture-directory>
voidc run <fixture/main.void> --no-opt
voidc run <fixture/main.void>
voidc run <fixture/main.void> --cache       # empty own scratch cache, then hits
voidc build <fixture/main.void> -o <scratch-native>
<scratch-native>
```

Use the supplied Python driver for per-child 1 MiB limits and incident capture.
Do not delete a user's project cache to simulate a cold case; each fixture owns
its scratch cache. `opt`/`llc` commands, host target details, helper source/build
commands, input hashes, phase inventories and complete pre/post IR are preserved.
Machine object sizes/hashes are retained; generated executables/objects and LTO
cache contents are omitted from the compact archive. No original crash artifacts
were produced by these successful bounded runs.

Unisolated areas: individual parsing/checking/specialization costs, ORC-only
linking, editor latency, real long-running async retention and actual-game
throughput. No Windows/macOS/ARM64 execution or across-commit regression test.
These are honest limits, not evidence that those components are defective or
free. Root scan and source shape evidence support focused next experiments;
the audit does not promise a measured speedup before a repair is implemented.

Current receipt count: **121 bounded root-driver operations**, all matching expected status; one is the deliberate status-2 CLI usage control. See final evidence manifest for the frozen count and provenance checks.
