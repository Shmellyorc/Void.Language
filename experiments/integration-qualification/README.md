# Integrated local qualification, 5 November 2026

> Publication note: this commit keeps selected text evidence. Full local archives,
> binaries, candidate trees and disposable runs remain excluded and preserved;
> see the [artifact scope and original-document hashes](../../docs/compiler-health/evidence/integrated-performance/LOCAL_ARTIFACTS.md).
> Measurements and conclusions below are unchanged; artifact links are portable.

This directory owns new evidence only. The logging/internal repair archives,
reports, workloads and raw measurements remain unchanged in their original
locations. No patch was applied during this qualification.

`qualify.py` freezes identity, builds the current targets, and runs the common
selection sequentially: 63 Release, 63 Debug, 62 strict. CMake excludes the
duplicate explicit text-stack wrapper under sanitizers; the ordinary strict text
execution inherits the same hard/soft 1 MiB limit. `supplement.py`
waits for each profile's saved common CTest log, builds the additional targets
and runs 22 boundary tests. Separate build directories can overlap; two CTest
runs never share a build directory. Logs are copied before another CTest run.
`prebuild_strict.py` prepares the independent strict tree while long Debug
executions run; it finishes before the main strict phase. `extra_editor.py` waits for Debug/strict common qualification before running the
preserving Stage 7 diagnostics. `diagnose.py` runs the original baseline/current
Stage 7 graph, HTTP IO diagnostic and separately linked historical advanced test.
The initially unavailable guessed baseline HTTP path is recorded; the retained
`io-control-baseline` host was then used under `http-baseline-retained`, once.
No failed execution was discarded or retried for a favorable time.

`commands.jsonl` records actual invocations, returns, wall/CPU, stack and strict
environment. Stdout/stderr, JUnit XML and saved CTest transcripts retain complete
results. `editor_capture.py` preserves sources, process limits, exits/stderr and
every completed timed action. Diagnostic timings under qualification load are
not a matched performance study; use the original retained ABBA evidence for
performance claims. The unchanged original advanced test is expected to fail
against this implementation; its failure is separate from semantic-test passes.

`finalize.py` checks the fixed completed campaign and records exact binaries and
recipes. `verify.py` verifies original artifact manifests, source identity,
protected README/Git state and newly added documentation links. Scripts refuse
to overwrite prior command outputs. Do not rerun the campaign in place: use a
new evidence directory/labels if Boss requests another campaign.

No SIMD code or SIMD benchmark, production code edit, Git mutation, hosted CI,
or release publication is included. Final report:
`docs/compiler-health/INTEGRATED_PERFORMANCE_QUALIFICATION.md`.
