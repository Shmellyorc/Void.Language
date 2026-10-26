# Crash audit and focused repair verification

26 October 2026. Baseline: `108d7639f1dda45df201ea950d9abd751d024ff7`.
Read-only production/evidence audit; no compiler or standard-library repair was
needed. This follow-up preserves the distinction between separate failures.

| Observation | Classification | Supported conclusion |
|---|---|---|
| Debug semantic-depth `incident-splghldg` | **Confirmed and repaired** | Original input, binary hash and debugger replay establish native stack exhaustion. Current cold-helper extraction reduces recursive frames and passes the unchanged depth/recovery tests at 1 MiB. |
| Historical Release filesystem `io_async_execution` | **Unresolved** | The original runner returned `-11` with empty output. Its temporary source and original executable hash were not retained. Later reconstructed-candidate passes do not identify the cause or establish repair. |
| Historical Debug LLVM 19.1.7 DSE observation | **Unresolved** | A retained debugger stack locates the failure during O2 optimization. Later identical runs and standalone optimization passed; neither an upstream LLVM defect nor sound in-process IR/analysis state is established. |
| Historical pinned Clang 19.1.7 collection-test build | **Unresolved** | A separate host C++ optimizer failure occurred in BDCE/DemandedBits. Unchanged source and flags succeeded on retry; no cause or repair is established. |
| Existing synthetic-preservation receipts | **Intentional test** | All 33 inspected receipts invoke Python with an explicit self-SIGSEGV command, including the stdin-consuming variant. Repeated standard-library snapshots are synthetic artifacts, not additional compiler crashes. |

## Confirmed Debug repair

[Original receipt](../../evidence/crashes/incident-splghldg/receipt.json) records
the exact command, cwd, `-11`/SIGSEGV, empty stdout/stderr, source companions and
original executable SHA-256
`f257c677824af8a9feba6c2bf31667dc2a7b9978a8c004109e76c4d411793a3d`.
The privately archived executable at
`build/text-encoding-crash-binaries/voidc-before-stack-repair` still matches it.
All 13 readonly source snapshots and 14 provenance hashes independently matched;
details and freshly inspected prologues are in [the audit receipt](crash-audit-readonly.json).

[The exact preserved fixture](../../evidence/crashes/incident-splghldg/inputs/deep-methods_crash.void)
is 3,016 bytes, SHA-256
`73871bef34c030e336699c8e7ff706917428d0450dfa5e7c7b923c9419245b00`.
It contains 400 `.same()` calls followed by `.inspect() + 1`, then a later
`missing()` diagnostic. The current semantic-depth generator exactly matches
its preserved pre-repair generator bytes.

[The retained GDB replay](../../evidence/stack-crash-gdb-before.txt), lines 4–51
and 108, shows recursive `anchor` frames and a fault at `anchor+11` with
RSP `0x7ffff485bc30`, below the mapped 1 MiB stack start `0x7ffff485c000`.
This establishes stack exhaustion in frontend hint analysis. It does not locate
a fault in GC, async execution, filesystem operations or LLVM optimization.

The complete diff between the preserved pre-repair compiler and current
`src/frontend/compiler.cpp` contains only the `name_anchor` and `text_case_call`
helper extractions. Both use the existing noinline handler attribute. Fresh
disassembly independently confirms stack subtraction reductions:

| Function | Original | Current Debug |
|---|---:|---:|
| `Semantics::anchor` | 3,840 bytes | 3,584 bytes |
| `Semantics::call_expression` | 19,904 bytes | 16,224 bytes |

`name_anchor` preserves global → local → constant resolution, the unresolved
global type check and the `!current` pure-expression guard. `anchor` retains its
256-level shared semantic budget and the existing diagnostic fallback. The
casing helper preserves the original target construction, parameter order and
argument checking. No stack reserve, guard limit, callback policy or exception
suppression was added by this repair. Rewriting it is unwarranted.

## Historical evidence limits

[The original filesystem log](../../../io/evidence/release-initial-io.txt), lines
25–41, retains the failure at `/tmp/VOID io async wire 1icf1laa/main.void` after
55.73 seconds. [The research report](../../../io/SIGSEGV_RESEARCH.md), lines
44–67, explicitly distinguishes original evidence from the 17,032-byte
reconstructed candidate, SHA-256
`64b6a7abbf71eb627d80a0244acb9a0db061e5f6c6f5d582e38732d510befaab`.
The original failing binary is not hash-preserved. [Bounded phase capture](../../../io/PHASE_CAPTURE_2026-10-26.md)
subsequently completed its declared five-attempt limit without a crash. This
audit did not repeat that workload or change GC, workers, LLVM or optimization.

[The optimizer report](../../../stage11/OPTIMIZER_FAILURE.md), lines 3–25, and
[original backtrace](../../../stage11/evidence/llvm-dse-backtrace.txt), lines
4–29, establish a distinct O2 compile failure in `CallBase::getMemoryEffects`
through `DSEPass`. The retained compressed preoptimization IR independently
decodes to its recorded SHA-256
`402019ad2887f892d65f8effa2aa516342d02013d19e54835187d4b0d2fe59e0`.
Passing retries and standalone `opt` do not establish a production repair.
Identical signal numbers are insufficient to connect these incidents.

### Additional retained compiler-tool observations

The collection-test build failure is a separate **unresolved** observation,
not an unsupported older-Clang incident. The preserved
[reproducer metadata](../../../stage10/engineering/collections-clang-crash-repro.cpp.json)
and [command](../../../stage10/engineering/collections-clang-crash-repro.sh.txt)
explicitly identify pinned Clang 19.1.7 compiling `tests/stage10/conversion.cpp`
with O2 and ASan/UBSan. [The original build log](../../../stage10/engineering/collections-san-proof-build.txt),
lines 28–37 and 72–73, records BDCE calling `DemandedBits::getDemandedBits` and
Clang frontend exit 139. [The unchanged retry](../../../stage10/engineering/collections-san-clang-reproduction.txt)
builds and links successfully. This is successful recovery by retry, not a
demonstrated repair or established root cause. Reading/decompressing the retained
archive independently verifies both metadata hashes: compressed SHA-256
`8a4d226b2c68f962198cae9f3418961a4abc69846e09dd735820e2200abb60c5`,
and 2,418,308 decoded bytes with SHA-256
`02139e5f89a39b5c5d1c7be206ca0da48e9f374f4055ce594685df8f68e2b52c`.

Two further records remain distinct: [the Stage 10 test plan](../../../stage10/TEST_PLAN.md),
lines 832–834, retains a Release LLD GVN stack emitted despite overall build
exit 0; its signal/cause remain **unresolved**, so the stack alone is not counted
as another proven SIGSEGV. [The Stage 11 implementation record](../../../stage11/IMPLEMENTATION.md),
lines 65–69, separately reports a build reading source during an in-place rewrite;
the recorded workflow mitigation is atomic file replacement. That engineering
failure is excluded from acceptance and does not explain the unchanged-input
BDCE, JIT DSE or filesystem observations. This appendix only inspected retained
records and archive bytes; no old compiler, debugger or workload was executed.

## Focused local verification

Actual registered CTests ran with `ctest -V -j 2` and this exact selection:

```text
^(text_encoding_semantic_depth|lsp_const_recovery|lsp_const_recovery_stack|lsp_protocol|lsp_stage11_globals|lsp_stage8|lsp_text_encoding)$
```

| Profile | Result | Wrapper wall time | Exact receipt |
|---|---|---:|---|
| Debug | 7/7 passed | 8.983 s | [Command/environment/hashes](crash-audit-debug-result.json), [verbose log](crash-audit-debug.log), [JUnit](crash-audit-debug-junit.xml) |
| Release | 7/7 passed | 1.868 s | [Command/environment/hashes](crash-audit-release-result.json), [verbose log](crash-audit-release.log), [JUnit](crash-audit-release-junit.xml) |
| Strict sanitizers | 6/6 passed | 6.328 s | [Command/environment/hashes](crash-audit-sanitizers-result.json), [verbose log](crash-audit-sanitizers.log), [JUnit](crash-audit-sanitizers-junit.xml) |

The full semantic-depth test processes 34 compiler cases per profile with an
actual Linux `RLIMIT_STACK` of 1,048,576 bytes, including legal receiver/numeric/
string chains through 124, excessive chains, nested blocks and recursive types.
It checks the original nesting locations and subsequent independent diagnostics.
Native const recovery passes 472 assertions both normally and with an actual
1 MiB server stack. Strict sanitizer registration runs the normal 472-assertion
const test, plus the actual 1 MiB semantic-depth test; the unregistered sanitizer
LSP stack variant is not claimed. The existing protocol, globals, import/docs/
readonly and text LSP tests verify unsaved malformed-source recovery, invalid
UTF-8 overlay recovery and repeated completion. The text test reports 54 checks.

Every test process inherited the absolute `tests/text_encoding` `PYTHONPATH` and
the genuine `docs/text-encoding/evidence/crashes` preservation root. Strict
execution retained `detect_leaks=1:halt_on_error=1:exitcode=87` and
`halt_on_error=1:print_stacktrace=1:exitcode=86`, with approved execution outside
the ptrace-restricted sandbox. No new genuine crash occurred; the genuine root
still contains only `incident-splghldg`. Original evidence was not edited.

These are focused local Linux results. No hosted CI, Git mutations, production
redesign or claim of a historical filesystem/optimizer fix is included.
