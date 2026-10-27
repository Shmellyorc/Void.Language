# Frozen compiler-health evidence

27 October 2026; baseline **`1d4ad15f5215838d209e48bb47ac7c3f52b05371`**.
Research artifacts only. [Measurements and methodology](../BENCHMARKS.md),
[interpretation](../REPORT.md), [engineering plan](../ROADMAP.md).

| Artifact | Contents |
|---|---|
| [benchmark-run.tar.xz](benchmark-run.tar.xz) | 121 bounded operation receipts, exact fixture sources, drivers, complete stdout/stderr, all pre/post LLVM IR, typed/LLVM inventories, target details and initial/final input manifests. |
| [phase-harness.tar.gz](phase-harness.tar.gz) | Scratch phase/API helper source, build scripts/commands/logs, dependency lists and source/library/binary hashes. Backend scratch TU compiled O1; existing core/Runtime Release. |
| [inference.tar.gz](inference.tar.gz) | Exact independent non-JSON reductions and 24 source-check receipts: 17 successes, seven reproduced source errors; arithmetic shape controls. |
| [root-lookup-harness.tar.gz](root-lookup-harness.tar.gz) | Runtime-only measurement source, build command and input/binary hashes. Its execution CSV is in benchmark-run's runtime-root-width stdout. |
| [measurements.jsonl](measurements.jsonl) | Directly inspectable raw root-driver receipts, also included in benchmark-run. |
| [FINAL_MANIFEST.json](FINAL_MANIFEST.json) | Frozen counts, source/hash verification, tool versions/hashes, pinned header witnesses and generated artifact sizes/hashes. |
| [ARCHIVES.json](ARCHIVES.json) | Archive SHA-256, byte sizes and member counts. Every archived member was reread and compared to its original content hash. |

**Outcome:** 121/121 root-driver operations matched their expected status
(including the intentional status-2 usage control). All 24 independent frontend
checks completed; seven intentionally expose current source-check defects.
No unexpected failure, signal, timeout or internal IR failure occurred.
All 134 recorded production input hashes and the unchanged compiler hash match
at the end. ASLR remained 2. Real 1 MiB child stacks were enforced.

No ptrace/debugger, hosted CI, production modification, branch, staging, commit
or push. Existing historical incidents were neither overwritten nor cleared.
This evidence does not establish sanitizer/native-platform acceptance.

## Extraction and reproduction

Inspect `ARCHIVES.json` and verify an archive hash before extraction. Extract
into a fresh scratch directory, for example:

```text
tar -xJf <evidence>/benchmark-run.tar.xz -C <fresh-scratch>
tar -xzf <evidence>/phase-harness.tar.gz -C <fresh-scratch>
tar -xzf <evidence>/inference.tar.gz -C <fresh-scratch>
tar -xzf <evidence>/root-lookup-harness.tar.gz -C <fresh-scratch>
```

Archives use relative `benchmark/`, `phase-harness/`, `inference/` and
`root-lookup/` prefixes. Original absolute machine paths remain in receipts and
build commands for provenance. Adjust path constants in scratch copies only,
retain original receipts, and use a fresh output directory for a new run.
`bench.py` expects `results/`, `fixtures/` and `phases/` beneath its directory;
it uses this repository's existing crash preservation helper. Review its finite
suite selection before rerunning expensive widths. Do not rerun automatically
merely because the evidence is being read.

Generated executables/objects and scratch ThinLTO/object-cache contents are
omitted to keep this evidence compact. Their relevant hashes/sizes and helper
rebuild commands are retained. LLVM tools are identified by exact version/hash;
no custom LLVM build was used. The root helper's `inputs.json` says
`executed:false` because it is the pre-execution build receipt; the subsequent
successful measurement is separately recorded in `measurements.jsonl`.

The `analyze` phase includes parsing, semantic checking and specialization.
Aggregate JIT compile includes codegen, ORC and metadata; only external identical
IR `llc` runs isolate machine-code generation. Do not subtract independently
built helpers to invent exact production subphase timings.

The original no-optimizer / optimized DTO history and all crash incident logs
remain in their existing `docs/json/`, `docs/stage11/`, `docs/io/` and related
directories, linked by [ISSUES](../ISSUES.md). No passing research execution is
claimed as a repair of those incidents.
