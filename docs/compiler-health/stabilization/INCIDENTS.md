# Stabilization failures and experiments

Original attempts remain available. A corrected rerun is evidence only for its
specified repair. The frozen local tree is `build/compiler-stabilization/`;
portable receipt/source/log archives and their hashes are indexed by the final
evidence manifest. No ptrace-based debugger or tracing was used.

| Observation | Classification, cause and retained disposition |
|---|---|
| First incremental constructor link failure | Build/source race: a public constructor declaration changed during an incremental build. `release-build-1.log` and build-attempt records retain the error. Stable-input rebuilding resolved the mismatch; no executed program or LLVM failure is inferred. |
| Candidate aggregate root proof SIGABRTs | Four unchanged independent proof observers rejected the broadened compound descriptor shape. Their noexcept hooks terminate on their explicit proof assertion. Exact executables/source/logs are in `first-aggregate-proof-failure/`. Packing is narrowed to pointer-shaped homes; aggregate/interface/prepared-receiver descriptors stay individual. All four unchanged proofs pass, including the prototype's 1 MiB runs and final three-profile selection. |
| Globals corruption test unexpectedly hit | The probe corrupted an unrelated first cache entry after automatic runs populated several objects. Its original mutation and required miss assertion now operate in an isolated project/cache. This was a test-targeting error, not cache acceptance of the damaged object. |
| Two initial proof reruns used stale binaries | Reference-values/composition targets had not been rebuilt. `proof-binary-provenance.json` retains the mismatches. The fresh six-test rerun and later complete target-derived rebuilds pass. Freshness and before/after hash stability are separate requirements. |
| Compound negative found no capture | The positive fixture uses a growable collection; lowering already uses ordinary stores, so searching for fixed-array `array_capture` mutated nothing. Independent removal of each actual compound capture store was rejected by the unchanged validator. The regression now requires each such rejection and at least one actual mutation. Original failure binaries/source/logs remain in `first-compound-test-failure/`. |
| Sanitized operator integration diagnosed unknown `_temporal_result` | An omitted target left an old frontend binary with the old `lower()` ABI against the current standard library. The old binary/symbols/source are in `first-stale-operator-integration/`. Every executable in the selected CTest commands is now explicitly rebuilt. The fresh target passes in all profiles. No temporal-library workaround was introduced. |
| Debug rejected deep receiver chain SIGSEGV | Confirmed native stack exhaustion, independently reproduced with the frozen baseline at 1 MiB. First receipt and exact source/imports: `docs/json/evidence/implementation/incidents/incident-agd8ryx5/`; executable/static assembly in `hint-stack-failure/`. The first projection-only reduction and second hint-dispatch reduction still failed; both source/binary attempts are frozen. Borrowed checking contexts plus outlined hints pass the original unchanged depth/recovery suite. See the engineering ledger for frame sizes. This does not explain historical H1–H7. |
| Directory glob provenance failures | Python `*.void`/`*.*` also selects the newly automatic `.void` directory. Regular-file enumeration now guards hashing and preservation; nonexistent files still fail. The synthetic regression includes a real cache directory and verifies exact bytes. Original failures remain; directory entries are not source files. |
| Superseded Debug acceptance was interrupted | Deliberate SIGINT after Release established the compound-packing proof failure. `debug-pass-controlled-stop.json` records it. The old preservation path then encountered the directory issue, leaving an incomplete incident directory under `docs/text-encoding/evidence/crashes/incident-3vzwyb7r/`. This is not an unexplained product signal or successful acceptance. |
| Superseded helper builds were stopped | Deliberate SIGTERM stops are recorded in `helper-attempt1-build-controlled-stop.json` and `helper-pipeline-stop.json`. The latter prevented a production target relink during Release acceptance; scratch helpers were rebuilt separately. Final acceptance confirms no production build file changed. |
| Scratch helper/fixture setup errors | Initial metering source omitted an explicit integer cast; a capture probe omitted LLVM Support linkage; the API helper omitted `runtime.hpp`; the first borrowed-context build retained an unused-variable suppression for a removed parameter. Original setup sources/logs are retained where produced. Corrected helpers/fixtures were rebuilt before measurement. They are not diagnosed Runtime or LLVM execution defects. |
| Initial baseline crash preservation lacked its manifest | The benchmark ledger and raw signal output were written first. The manifest was added and complete preservation repeated with the same frozen baseline image and source. Both attempts remain; the baseline stack failure is independently captured. |
| Fixed-value encoder choice copying | Source inspection identified extra owned-array snapshots through nested choice joins. Fixed/value fields retain their original per-field conversion path; one-field groups avoid the extra selected temporary. No array-copy contract was weakened. |

The final clean acceptance receipts are linked from [verification](VERIFICATION.md).
Historical DSE, Release async I/O, Stage 9 process, pinned Clang and linker/cache
observations retain their separate unresolved classifications in the
[active inventory](../ISSUES.md). Passing current workloads does not establish
why any unexplained historical run failed.

The allocation-counting API helper initially exited 4 before JIT configuration.
Its first binary/source/support object and receipt are retained in
`first-api-allocation-probe-failure/` and measurement incident `incident-0m_xfk_j`.
An ordinary diagnostic build reported `bytes=512 alignment=4 status=22`:
LLVM legitimately requests four-byte aligned allocation, while the interposer
passed that directly to POSIX `posix_memalign`, which requires at least pointer
alignment. The shim now raises the physical alignment to at least `sizeof(void*)`,
satisfying the original request and preserving requested-byte accounting. The
separate diagnostic source/binary and `incident-6rkj2qjf` are also retained.
This is a measurement-interposer defect, not production allocator exhaustion,
a Runtime failure or an LLVM crash. No production source was changed.

An extra async all/entry-emission helper first passed Release/Debug, then UBSan
reported invalid `std::_Sp_counted_base` vptr metadata in its sanitized main.
The ad hoc compile omitted the project's required `-fno-rtti`, enabling RTTI/vptr
instrumentation against RTTI-disabled project libraries. The first helper image,
source, inputs, build commands and actual configured flags are frozen under
`first-entry-async-san-failure/`, with measurement incident `incident-7unl29ch`.
The helper is rebuilt using each profile's exact CMake-generated compile/link
flags. Existing project sanitizer flags and production source are unchanged;
this is separate from the already-passing 93-test strict production selection.
