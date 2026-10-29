# Bounded migration measurements

`metrics.cpp` is a private diagnostic host, compiled twice against the preserved
LLVM19 and migrated LLVM23 Release archives using their recorded existing link
inputs (`../metric-host-builds-final.json`). It uses public compiler APIs and
separately measures analysis, typed lowering, JIT construction, LLVM compilation/
ORC linking, three checked execution samples and teardown. These are full-module
embedding observations with no persistent object cache, not the entry-pruned warm
CLI latency. No production phase instrumentation is added. Runtime/source stdlib
inputs are identical across each comparison; compiler/LLVM archives differ.

Run from the repository only after builds and acceptance suites are idle:

```sh
python3 docs/llvm23/evidence/metrics/run_metrics.py
python3 docs/llvm23/evidence/metrics/run_dse.py
```

`run_metrics.py` has exactly 20 child measurements: five source workloads times
normal/O2 times two toolchains, three in-process execution samples each. It
alternates toolchain order by mode. Each child is limited to 600 seconds and has
actual hard and soft 1 MiB stack limits. Inputs, expected independent checksums,
host identities, stdout/stderr and measured wall/CPU/RSS are retained. No statistical
confidence or cold filesystem guarantee is inferred from one compilation sample.
`../metrics-source-checks.json` preserves initial preparation errors; the temporary
logging variable was renamed after both compilers correctly rejected shadowing.

`run_dse.py` performs exactly two retained-input standard no-TM O2 IR replays with
pre/post verification, followed by one O2 compilation and three execution samples
of the source-associated logging fixture. Its retained files are associated
diagnostic inputs; the original crashing optimizer heap and pass state are not
reconstructed. No original LLVM19 crash campaign is repeated.

The existing shared incident recorder is reused. An abnormal exit or timeout stops
the bounded measurement immediately, preserves exact source snapshots and leaves
the private working directory/core in place. Successful workspaces are removed.
The scripts enable process-local core capture (`0x37` filter); they do not use
ptrace or change global core settings. Absence of a crash establishes only this
bounded outcome, not the historical root cause or repair.

Before any timed execution, the numeric input was set to one million iterations
(independent checksum 499,501,000) and text to the existing benchmark's 16,384-unit
case (65,536 UTF-8 input bytes). This reduces tiny-sample timer noise without
changing source implementations or compilation counts. The earlier preparation
parameters remain in `cases-preparation.json`; neither configuration is a
historical performance result.

The accepted run is complete: 20/20 representative measurements and 3/3 bounded
DSE-associated checks passed. `comparison.json`/`comparison.csv` retain every
slower point as well as improvements; the acceptance report states the sampling
limits. The replay helper restores the retained original module identifier before
verification. The original crash state remains unavailable and no repair is proved.

The commands above document the executed run. For an independent repeat, first
copy this directory to a fresh sibling directory at the same depth and invoke
those copies, preserving accepted results here. Both drivers write their output
beside their own script; use the recorded matching measurement binaries and inputs.
