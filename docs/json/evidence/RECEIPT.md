# JSON research capability evidence

27 October 2026; baseline `194603ea00eb38aff72b7d6e950865b3a23df92a`.

[Complete original scratch evidence](capability-probes.tar.gz) contains 378
files, including every fixture, saved stdout/stderr, four probe reports and all
six unexpected IR incident directories. [Manifest](capability-probes.json)
records every file's size/hash and both compiler identities. All archive entries
were read back and matched against the manifest; no extraction/execution is
required to inspect it. Archive SHA-256:
`577b441f9a3e451913eae6fb3c9b5ee8391b62567a35e1682eeaa81c92c5c9b0`.

There were **37 bounded compiler processes**: 18 status 0, 19 status 1. Of these,
12 were successful JIT executions, six were successful source checks and six were
unexpected `VOID IR:` failures. The remaining 13 status-1 outcomes include
expected unsupported-language diagnostics and the retained initial fixture
mistakes. They are not 37 passing JSON tests. Corrected fixtures have different
names; originals and erroneous expectations are retained in `report.json`.

The records are `report.json` (12 processes), `followup.json` (14),
`readonly-isolation.json` (8) and `cross-module.json` (3). All children ran with
an actual 1,048,576-byte RLIMIT_STACK and a 90-second timeout. No timeout or
signal occurred. Existing Debug/Release binaries were not rebuilt or newly
source-frozen; these are observations of identified binaries with a separate
current-source audit. No JSON implementation, CTests, sanitizers, linked AOT,
hosted CI or native Windows/macOS runs are claimed.

Six incident archives preserve exact `_crash.void` bytes and existing harness
metadata. Those paths originally referenced the scratch directory and are
retained unchanged as historical records. Manifest paths identify their actual
archive locations. The first two incidents were preserved from original captured
stdout/stderr after observing their status-1 internal errors, before any further
probe; they are not fabricated signal captures. No standalone original generator
script was saved: fixture generation commands are in the investigation transcript.

The starting untracked `docs/math/evidence/reruns/` directory was left unchanged.
This research does not establish the cause or repair of earlier filesystem,
optimizer or Random incidents. See [findings](../RESEARCH.md) for interpretation.
