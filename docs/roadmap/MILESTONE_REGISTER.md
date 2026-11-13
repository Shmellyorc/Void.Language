# VOID developer experience milestone register

Current authorized work is **G26: package add, project libraries and tidy**, starting
at `49e419b7985c5e10234d158533d18146753d3308`. G23/G24/G25 are integrated
in that baseline. G26 reuses local libraries and D3/D4/D5; implementation and
qualification are recorded in the [G26 report](../projects/G26_PACKAGE_MAINTENANCE.md).
Boss controls integration, publication and hosted CI.

The preceding assignment was the Boss-authorized G23 editor packaging and G24 raylib
verified source package integration. G22 is accepted at
`ff3bbcd5a20fa2e81506d23e276a8a7837e9404f`. G23 is implemented; G24 has usable
interfaces for all 600 declarations, with typed adaptations and native limits
explicit. The G24 physical-source conversion is implemented locally; canonical
development stays in `verified-packages/raylib/`, with a standalone Git subtree
publication proposal awaiting Boss authorization. Boss acceptance remains pending.
Public post-push qualification at `6a7cb50b2d35023324053694275596c9a4a7b328` failed
on unchanged D3 whole-repository acquisition limits; see the
[public-origin report](../projects/G24_PUBLIC_GITHUB_QUALIFICATION.md) and
[physical-source report](../projects/G24_PHYSICAL_SOURCE_IMPLEMENTATION.md).
Batch entries below identify the integration commits and qualification scopes.
Boss controls integration, hosted qualification and public release.
[Feature roadmap](FEATURE_COMPLETENESS_AND_DEVELOPER_EXPERIENCE.md)

## Integrated milestones

| Batch | Integration commit | Qualification and boundaries |
| --- | --- | --- |
| 1 · G13/G01/G02 | `29d50c9a21cdf93579941b872ef815b2ae7f881a` | Local PikaOS and Windows 10 Debug/Release CTest and VS Code. Unreferenced buffers stay outside execution. Rename diagnostics recover in the same session. Check/Build/Run require saved source and manifest buffers. Native authorization and interactive TTY use remain CLI workflows. [Record](FEATURE_COMPLETENESS_AND_DEVELOPER_EXPERIENCE.md#batch-1-implementation-record) |
| 2 · G03 | `6ef25f38644b326a398bb334ba0da4954d57ca4c` | Local Linux LLDB and Windows GDB AOT, plus Linux VS Code LLDB DAP. Strict Linux control covers source-debug emission. DWARF line/full and supported O0 locals qualified. Windows PDB/IDE debugging and JIT source debug remain outside this scope. [Record](FEATURE_COMPLETENESS_AND_DEVELOPER_EXPERIENCE.md#batch-2-native-source-debugging) |
| G14 | `131db64368751f9fdac5608aba11f3241dd9ba6d` | Local PikaOS and Windows 10 Debug/Release native and VS Code recovery. The VSIX fix preserves the installed compiler/server pair. Compiler recovery remains bounded and can withhold results for unresolved source. [Record](FEATURE_COMPLETENESS_AND_DEVELOPER_EXPERIENCE.md#g14-editor-intelligence-recovery-and-vsix-packaging) |
| 3 · G04 | `77986362acf8b40d52aacbbdb324665b4484155b` | Local Linux Debug/Release/strict and Windows 10 Debug/Release. Bounded synchronous/async child capture, deadlines, cancellation and retirement. Existing inherited-stream execution remains available. [Record](FEATURE_COMPLETENESS_AND_DEVELOPER_EXPERIENCE.md#g04-bounded-subprocess-capture-deadlines-and-cancellation) |
| 4 · G07 | `bf5e6dc0c335b82e11df81cb540a6364110d3523` | Local Linux Debug/Release/strict and Windows 10 Debug/Release. Numeric addresses, bounded DNS, TCP listeners/streams and unicast UDP with native sync/async completion. macOS socket qualification remains open. [Record](FEATURE_COMPLETENESS_AND_DEVELOPER_EXPERIENCE.md#g07-native-tcp-udp-and-dns) |
| 5 · G15 | `1e988f00e41f3d3433859e756737acc25d3bf35c` | Local Linux Debug/Release/strict and Windows 10 Debug/Release after correcting the obsolete tab-indentation test. Named/unnamed returns and ordered destructuring retain the typed ABI. First-class tuples and async/function-value multiple returns are outside scope. [Record](FEATURE_COMPLETENESS_AND_DEVELOPER_EXPERIENCE.md#g15-native-qualification) |
| 6 · G05 | `7af58a3ad4ebee4e0bc6c9a97c23f356ea2bcd4c` | Local Linux Debug/Release/strict and Windows 10 Debug/Release. Verified read-only dependency explanation and guarded VS Code quick fixes. Vim/Neovim manifest parity remains open. [Record](FEATURE_COMPLETENESS_AND_DEVELOPER_EXPERIENCE.md#g05-dependency-intelligence-and-guarded-maintenance) |
| 7 · G16 | `6b28c57ff50256734fb804387f7c0ffca828d51f` | Local Linux Debug/Release/strict and Windows 10 Debug/Release. Compiler-owned enum completion and semantic roles respect visibility and stale-result guards. Recovery remains limited to supported incomplete contexts. [Record](FEATURE_COMPLETENESS_AND_DEVELOPER_EXPERIENCE.md#g16-enum-completion-and-semantic-highlighting) |
| 8 · G06 | `561bd020d9904dcfa2dc792297f9e87688e89f5e` | New save tests passed on all five local profiles. The two existing Windows I/O source suites failed the file-symlink prerequisite at check 91. The separate native control observed error 1314 and verified junction refusal. The synchronous API uses admitted local filesystems and a held private directory. Async saves and macOS remain outside scope. [Record](FEATURE_COMPLETENESS_AND_DEVELOPER_EXPERIENCE.md#g06-atomic-application-saves) · [Incident and replay boundary](../release/G00_READINESS_AUDIT.md#windows-file-symlink-incident) |
| 9 · G08 | `8b714ebb525fa7c822e7f7c35780b58f9f48208c` | **BASELINE MEASURED / OPTIMIZATION HOLD.** Local Linux and Windows 10 Debug/Release timing and strict Linux correctness. A revision-only completion cache would miss a changed dependency. Windows persistent caching remains an architecture decision. [Qualification](../compiler-health/G08_COMPILER_PERFORMANCE.md) |
| 10 · G09 | `e83c1070e5d2f6b49a2b84c52e21fbf2e6e319b1` | **INTEGRATED / GOLDEN WRENCH SOURCE ACCEPTED.** Bounded JSON configuration, explicit typed field defaults and G06 atomic save/reload. Local PikaOS Debug/Release/strict and Windows 10 Debug/Release. Other data and text conveniences remain separate. [Qualification](../json/IMPLEMENTATION.md#g09-configuration-and-typed-field-defaults) |
| 11 · G10 | `ceefd3f9a75257756cb8ab476868731e4359dcca` | **INTEGRATED.** Bounded monotonic sleep and structured cancellation/drain. Local PikaOS Debug/Release/strict and Windows 10 Debug/Release. General deadline composition and async queue/backpressure remain deferred. [Qualification](../ASYNC.md#g10-monotonic-timers) |
| 12 · G11 | `79f212f2907b3fb5eeeabebb0aa8fa5705b82c86` | **INTEGRATED.** Real Windows managed raylib and D5 headless execution. The separate G5 retained-callback fixture passed within its contract. The retained raylib callback remains on HOLD. macOS managed native and D5 remain unsupported. Signed App Sandbox remains unverified. [Qualification](../projects/G11_WINDOWS_MANAGED_NATIVE_QUALIFICATION.md) |
| 13 · G12 | `afe14d4c0cd7fd0576ecc194a38c294d7e54ec0a` | **INTEGRATED / INDEPENDENT SOURCE REVIEW COMPLETE.** Bounded deterministic stored ZIP with G06 publication. Both archive tests passed on PikaOS Debug/Release/strict and Windows 10 Debug/Release. The unrelated Windows file-symlink control remains open. [Qualification](../archive/ZIP_EXPORT.md) |
| 14 · G00-A | `d453195bfcbd34db211f4dddd8fa0832acbf8733` | **AUDIT INTEGRATED / INDEPENDENT REVIEW COMPLETE.** The distribution HOLD and deferred seven-job project/raylib gate remain. [Readiness audit](../release/G00_READINESS_AUDIT.md) |
| 15 · G17 | `35406a00aee892def5a0a7555c5beeae6fe284b3` | **INTEGRATED / INDEPENDENT REVIEW COMPLETE.** Five-module ASCII Adventure, deterministic JIT and AOT gameplay, and physical keyboard qualification on PikaOS and Windows 10. [Qualification](../developer-experience/G17_ASCII_ADVENTURE.md) |
| 16 · G18 | `3c81b7eb75c44533a5326da589a6fe47144c3c0f` | **INTEGRATED / INDEPENDENT SOURCE REVIEW COMPLETE**, as reconciled by the G19 assignment. Tagged-enum comparison guidance preserves original error codes/spans and guarded fixes. The separate Windows executable-cleanup refusal remains unexplained. [Qualification](../developer-experience/G18_ENUM_EQUALITY_DIAGNOSTICS.md) |
| 17 · G19 | `bd6e0ebe6e43dc26d9a4ce1258d85739806d22e1` | **INTEGRATED / INDEPENDENT SOURCE REVIEW COMPLETE. PRODUCTION OPTIMIZATION HOLD.** The deferred-capture candidate failed root-error equivalence. The timings, root-disappearance transcript and five original 15/16 results remain in the G19 record. [Attribution and experiment](../compiler-health/G19_COMPLETION_ATTRIBUTION.md) |
| G21-R | `c80e686a30e83ebf154985dcd00f5f40296882f8` | **RESEARCH ACCEPTED / INTEGRATED.** The Stage 7 assertion predated G13 open-document ownership. The rejected graph-only publisher hid valid independent diagnostics, including the undeclared-package error. The research preserves the five experimental profiles and the VS Code visual-close observation. [Research](../compiler-health/G21_LSP_DIAGNOSTIC_LIFECYCLE_RESEARCH.md) |
| 19 · G21 | `59db769cacd80ceba60d3bf9041007d21f300428` | **ACCEPTED / INTEGRATED.** The complete legacy protocol follows G13 diagnostic ownership. Maintained tests cover open and disk ownership transitions. All five native profiles passed the 17-case selection. [Qualification](../compiler-health/G21_LSP_DIAGNOSTIC_LIFECYCLE_QUALIFICATION.md) |
| G22 | `ff3bbcd5a20fa2e81506d23e276a8a7837e9404f` | **ACCEPTED / INTEGRATED**, as authorized by the G23/G24 assignment. Watched-event refresh preserves open buffers; explicit rename, delete and close preserve ownership transitions. [Qualification](../compiler-health/G22_WATCHER_OVERLAY_RESEARCH.md#authorized-production-repair) |

## Current and upcoming

| Milestone | Status and next evidence |
| --- | --- |
| 18 · G20 | **ACCEPTED**, as confirmed by the G21-R assignment. Integrated at `49d2e66243d80957e7dd1bbeecb9767a43a13964`. Physical-root recovery and established tabs formatting remain preserved. [Qualification](../compiler-health/G20_PROJECT_ROOT_RECOVERY.md) |
| G23 | **IMPLEMENTED / LOCAL EDITOR QUALIFICATION.** Package reuses Check/Build/Run selection, trust, saved-buffer, profile and compiler execution checks. Real package verification passed in native VS Code 1.141.0 on PikaOS and Windows 10 Debug/Release. No integration commit created. [Implementation](../projects/G23_G24_IMPLEMENTATION.md) |
| G24 | **IMPLEMENTED; PHYSICAL SOURCE LOCAL; STANDALONE PUBLICATION PENDING.** Baseline `6a7cb50b2d35023324053694275596c9a4a7b328` preserves all 600 usable interfaces: 236 direct, 352 ownership, ten G5 lifecycle and two typed equivalents. Real `.void` public/native modules replace raylib ghost source; editable behavior and root-repository Git consumers are locally qualified. The actual monorepo GitHub failure on all five builds remains recorded; D3 limits are unchanged. Reproducible subtree publication is proposed, with no repository creation/push authorized or performed. Strict external device leaks remain unsuppressed. [Physical-source report and publication commands](../projects/G24_PHYSICAL_SOURCE_IMPLEMENTATION.md) · [Public-origin qualification](../projects/G24_PUBLIC_GITHUB_QUALIFICATION.md) · [Integrated implementation](../projects/G23_G24_IMPLEMENTATION.md) |
| G25 | **IMPLEMENTED LOCALLY / FIVE NATIVE PROFILES QUALIFIED.** Explicit `voidc native import` derives editable physical VOID, conventional descriptors, coverage and value adapters with pinned Clang. Pointer ownership/callbacks remain reviewed G3/G4/G5 contracts. Independent C/C++/SQLite and five-profile qualification are recorded in the [G25 report](../projects/G25_NATIVE_HEADER_IMPORTER.md). No new FFI, loader, registry or Runtime; no staging/commit/push/hosted CI. |
| G26 | **IMPLEMENTED LOCALLY / FIVE NATIVE PROFILES QUALIFIED.** Explicit `voidc add`, `voidc tidy` and read-only `tidy --check`; editable path sources and verified Git views under `libs/`, existing package identities and D3 locks, recoverable serialized publication, separate D4 approval. See the [G26 report](../projects/G26_PACKAGE_MAINTENANCE.md). No registry, downloader, Runtime, compiler ABI or D3 limit change. |
| G00 · Production distribution | Conditional on release authorization. Public bootstrap, distribution and platform obligations remain tracked in the release record. [Preview blockers](../release/PREVIEW_BLOCKERS.md) |



## Hosted acceptance

| Focused hosted acceptance | Run and tested commit |
| --- | --- |
| W1 Windows Server 2022 x64 native projects | `42986407961` at `01c465d444060fae1e2806a21673db5ccacfcc8f` |
| W2 Windows Server 2022 x64 application packaging | `43069968456` at `5fffda8f406e23507367b19152fb8fdce3483522` |
| macOS ARM64 application context and editor | `43101701646` at `31608c2dd2ca1ae20e3bdd021d0c36581a01dabc` |

The original integrated project/raylib seven-job gate remains
**OPEN / DEFERRED BY BOSS DECISION**.
[Accepted scopes and deferred gate](../projects/PROJECT_APPLICATION_ACCEPTANCE_RECONCILIATION.md)
