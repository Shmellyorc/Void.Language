# Logging failure record

Original evidence is immutable under [evidence/crashes](evidence/crashes/).
The existing `tests/text_encoding/crash.py` captured launch-time `_crash.void`
sources, imports, generator/native-test provenance, command/cwd, return status,
stdout/stderr and source/tool hashes. [The machine-readable index](evidence/incident-index.json)
lists every preserved receipt; aggregate CTest failures are separate receipts,
not additional independently diagnosed defects. Expected negative source tests
remain normal successful test cases.

| Original incident(s) | Observation and disposition |
| --- | --- |
| [9rgwn6ff](evidence/crashes/incident-9rgwn6ff/receipt.json) | Default construction incorrectly required explicitly defaulted enum fields. Shared frontend fix recognizes complete enum/callable field defaults; positive nested/managed/value tests and missing-default rejections remain. Ordinary source diagnostic, not a native crash. |
| [2kx9na4j](evidence/crashes/incident-2kx9na4j/receipt.json), [4m33gynx](evidence/crashes/incident-4m33gynx/receipt.json) | Source identifier and readonly-cause-forwarding diagnostics. Corrected source within existing contracts. |
| [0qchv_td](evidence/crashes/incident-0qchv_td/receipt.json) | The runner's second mode reused the first mode's files; reject correctly returned an existing-file error. Fixture now removes only its exact test-owned files between invocations. |
| [65bedlo7](evidence/crashes/incident-65bedlo7/receipt.json), [nvvqkc4a](evidence/crashes/incident-nvvqkc4a/receipt.json), [t7g8xkin](evidence/crashes/incident-t7g8xkin/receipt.json) | Fixture numeric-format syntax, local shadowing, and source private-field access diagnostics. Fixed using the existing formatting grammar, distinct local name and owning receiver helper. |
| [wyrg8ihl](evidence/crashes/incident-wyrg8ihl/receipt.json), [ur4a6o8g](evidence/crashes/incident-ur4a6o8g/receipt.json) | Injected native allocation failures surfaced through existing string/UTF-8 Runtime diagnostics. The original sweep did not recognize those diagnostic wrappers. The corrected harness accepts those specific injected failures and still requires balanced roots, cleanup and eventual success. |
| [lcvquujb](evidence/crashes/incident-lcvquujb/receipt.json) | The initial allocation sweep never reached success. Its bounded budget was extended through 65,536; final runs observe 71 failures followed by success per mode. |
| [0c6l8m0t](evidence/crashes/incident-0c6l8m0t/receipt.json), [je6m9xz_](evidence/crashes/incident-je6m9xz_/receipt.json) | A supposed post-write failure had not actually been injected. The instrumented observation was `failed=0 state=7 bytes=32`. A real second-chunk allocation failure now leaves one native byte, faults the logger and rejects replay. The earlier assertion alone did not demonstrate an unsafe retry. |
| [bb5x686j](evidence/crashes/incident-bb5x686j/receipt.json), [9tzv4lmi](evidence/crashes/incident-9tzv4lmi/receipt.json), [g4r83x4x](evidence/crashes/incident-g4r83x4x/receipt.json), [7rh_igd0](evidence/crashes/incident-7rh_igd0/receipt.json) | Cancelled acquisition did not close before Runtime teardown. The last, instrumented AOT observation established descriptors 4→5, an empty created file and zero explicit closes. Logger guards now retain the prepared `io.file` identity before awaiting acquisition. Provider ownership already existed; the repair supplies deterministic source cleanup. |
| [piwcm854](evidence/crashes/incident-piwcm854/receipt.json), [5zapu4ss](evidence/crashes/incident-5zapu4ss/receipt.json) | The assertion expected faulted after cancelled close had retired the native handle. Correct state is closed. The corrected test requires closed for retired close and faulted for uncertain write/rotation; it does not accept arbitrary failure states. The second receipt is its CTest aggregate. |
| [67zzofyr](evidence/crashes/incident-67zzofyr/receipt.json), [rs9b9ass](evidence/crashes/incident-rs9b9ass/receipt.json), [pno1n6kl](evidence/crashes/incident-pno1n6kl/receipt.json), [zg7ztvpg](evidence/crashes/incident-zg7ztvpg/receipt.json) | Forced-GC cancellation rejected the expected cancellation outcome. The instrumented rerun recorded `Async await dependency cycle`. The last receipt is the superseded Release aggregate. |
| [ej4p6can](evidence/crashes/incident-ej4p6can/receipt.json) | A deterministic existing-native-job regression reproduced that cycle without logging. A child can finish draining after its parent was checked in the same scheduler scan. The shared scheduler now rescans after retirement before declaring a cycle. The regression requires actual cancellation and native abandonment, with zero retained task roots. |
| [189q5thv](evidence/crashes/incident-189q5thv/receipt.json), [41ex8kof](evidence/crashes/incident-41ex8kof/receipt.json), [cjie5tta](evidence/crashes/incident-cjie5tta/receipt.json) | Deliberate SIGTERM of the in-progress superseded strict native host and Debug/strict CTest orchestration before rebuilding. [Stop journal](evidence/superseded-profile-stop.json) and [native files](evidence/stopped-native-1466352/) preserve the observation. The host had progressed into its second optimization mode. The journal's noncompletion label describes its state when stopped; no spontaneous crash or hang is established. |
| [rp2_cl9p](evidence/crashes/incident-rp2_cl9p/receipt.json), [06ywm3_e](evidence/crashes/incident-06ywm3_e/receipt.json) | Deliberate stop of the intermediate Runtime(0) benchmark after its JIT part completed. Budget zero disables automatic GC; normal JIT/AOT creation uses 256. The second receipt is the outer benchmark driver. All 157 complete observed samples and the original host overlay are [preserved](evidence/benchmarks-gc-unbudgeted-partial.json); the final 216 timing samples use the verified production budget. No logger failure is inferred. |

The new acquisition and scheduler defects above are distinct from historical
H1–H7. Those historical incidents remain unresolved as recorded in the existing
compiler-health documents. No ptrace, debugger or hosted CI was used in this sprint.
Independent Golden Wrench review remains required, including review of the two
shared fixes and all preserved original observations.

## Mandatory performance repair incidents

The original recorder snapshots remain unchanged. Fresh final indexes will include
these alongside the historical incidents above.

| Incident | Observation and disposition |
|---|---|
| [gfpyif5z](evidence/crashes/incident-gfpyif5z/receipt.json) | Test-only component overlay had an import-placement/local type-name collision. Corrected fixture; ordinary source diagnostic. |
| [3udty23f](evidence/crashes/incident-3udty23f/receipt.json) | Initial escape replacement used unsupported multiline array syntax. Corrected source syntax; original rejected fixture retained. |
| [d4b4kyqi](evidence/crashes/incident-d4b4kyqi/receipt.json) | Async fixture parameter shadowed the imported file type. Renamed the parameter; ordinary source diagnostic. |
| [2psi0ciz](evidence/crashes/incident-2psi0ciz/receipt.json) | Build command used `void_growable_arrays` instead of `void_growable_arrays_tests`. Status 2; corrected target built. |
| [v917yx1_](evidence/crashes/incident-v917yx1_/receipt.json), [du4yxvhw](evidence/crashes/incident-du4yxvhw/receipt.json) | Injected allocation failure reached the existing scalar-display wrapper through final record interpolation. The logging host's expected diagnostic list omitted `Display operation failed`. Added that specific wrapper while retaining rooting, ownership, state, partial-effect and recovery checks. Focused retry passed 748 checks, with 71 acquisition failures per JIT mode. The second incident is the enclosing CTest report. |
