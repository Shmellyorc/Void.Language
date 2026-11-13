# Development strategy and current status

## Current project/application work, 13 November 2026

Project manifests, compiler-owned entry discovery, project sessions, application
metadata, init/new, numeric enums, typed C imports and native packaging are
implemented through the existing frontend, typed IR, Runtime and LLVM backend.
G23 editor packaging, G24 physical raylib source, G25 header import and G26 package
maintenance are integrated at the G27 baseline
`5aa51bdd1edbf1146fb64379497b2a1fb0b78561`.

G27 completes library authoring: generated library manifests carry the known
`package.entry`, and `add` supports both application and library consumers.
[Library workflow](LOCAL_LIBRARIES.md#creating-and-composing-libraries) ·
[G27 implementation and qualification](projects/G27_LIBRARY_AUTHORING.md).
The [architecture roadmap](PROJECT_ARCHITECTURE_ROADMAP.md) retains design history;
its original pending slices are not today's implementation status. Release and
platform qualification remain limited to their recorded scopes.

## Current implementation status

The active toolchain is **LLVM/Clang/LLD 23.1.4 exclusively**; use the
[toolchain requirements](SOURCE_COMPILER.md#active-toolchain). Existing frontend, typed IR, LLVM JIT/AOT
backend, Runtime resource ledger, precise GC and source Task architecture remain
shared. Current API guides determine behavior; proposals record design history.

| Area | Current engineering record |
| --- | --- |
| Project/library/native workflow | [Milestone register](roadmap/MILESTONE_REGISTER.md), [library authoring](LOCAL_LIBRARIES.md), [G25](projects/G25_NATIVE_HEADER_IMPORTER.md), [G26](projects/G26_PACKAGE_MAINTENANCE.md), [G27](projects/G27_LIBRARY_AUTHORING.md) |
| Language/compiler and entry semantics | [Source compiler](SOURCE_COMPILER.md), [CLI](CLI.md), [language guide](LANGUAGE_BIBLE.md) |
| LLVM migration | [Compatibility](llvm23/COMPATIBILITY.md), [acceptance](llvm23/ACCEPTANCE.md); historical hosted checkpoint is distinct from this repair |
| HTTP | [Current engineering](net/http/CURRENT_ENGINEERING.md), [accepted hosted scope](net/http/ACCEPTANCE.md) |
| VOIDUP A/B | [Phase A](voidup/REPORT.md), [Phase B](voidup/PHASE_B_REPORT.md), [integration](voidup/INTEGRATION_REPORT.md); private Linux engineering retained |
| VOIDUP CI repair scope | [LLVM selector and sanitizer investigation](voidup/INTEGRATION_REPORT.md#llvm-2314-ci-repair); that record's native evidence and strict libsodium limits are distinct from later project/compiler qualification |
| Performance | [Integrated qualification](compiler-health/INTEGRATED_PERFORMANCE_QUALIFICATION.md), with original incidents and regressions retained |
| Release/installation limits | [Preview blockers](release/PREVIEW_BLOCKERS.md), [installation](install/README.md), [licensing](release/REDISTRIBUTION_AUDIT.md) |
| TCP/UDP/DNS | [Integrated G07 implementation and native qualification](roadmap/FEATURE_COMPLETENESS_AND_DEVELOPER_EXPERIENCE.md#g07-native-tcp-udp-and-dns); current macOS socket qualification remains open |

Stage-specific evidence remains in the [historical index](README.md#historical-records).
The [previous strategy record](https://github.com/Shmellyorc/Void.Language/blob/82e97f541fab7eec760083383569ab9c9ce8f355/docs/DEVELOPMENT_STRATEGY.md)
preserves original approvals, counts and timelines. Its dated pending-stage
statements do not override later acceptance or current Boss authorization.
Passing CI in an earlier scope is not acceptance of today's untested changes.

## Future optimization: automatic SIMD — Boss direction, 5 November 2026

The [AOT-only candidate](compiler-health/AUTOMATIC_SIMD_AOT_ONLY_QUALIFICATION.md)
was retained for cross-platform qualification; the prior global JIT+AOT trial was
rejected because measured cold O2 execution regressed. Preserve both records.
[Staging scope](compiler-health/AUTOMATIC_SIMD_AOT_STAGING.md) and the
[detailed roadmap](VOIDC_AUTOMATIC_SIMD_ROADMAP.md) retain the actual boundaries.
Broader SIMD work is outside G27 library authoring. The old FILE200
10 ms aim is not a Boss-mandated release threshold.

## Owner workflow and Stage 7 planning — 19 October 2026

The current role/workflow instructions are maintained once in
[Wrench](WRENCH_CLANKER.md#permanent-workflow-for-every-new-development-stage)
and [Research](RESEARCH_CLANKER.md). The old stage-planning text and original
owner decisions are [archived](https://github.com/Shmellyorc/Void.Language/blob/82e97f541fab7eec760083383569ab9c9ce8f355/docs/DEVELOPMENT_STRATEGY.md#owner-workflow-and-stage-7-planning--19-october-2026).
Boss alone controls Git integration, hosted CI and publication. This document
does not authorize another feature sprint or create a new review requirement.
