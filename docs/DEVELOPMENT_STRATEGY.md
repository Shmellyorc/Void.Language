# Development strategy and current status

## Next project/application priorities, 8 November 2026

The [coordinated architecture roadmap](PROJECT_ARCHITECTURE_ROADMAP.md) records
Boss and Golden Wrench's approved direction, proposed details, baseline audit,
unresolved questions and acceptance gates. Recommended order: schema/identity and
compiler discovery; embedded metadata/OS conveniences; init/new/editor creation;
checked numeric enums; resolver/general typed C ABI; dynamic raylib then optional
static; debugger, platform packaging and public onboarding/release preparation.
Some language work can proceed independently after review. All new features remain
unimplemented/unverified; this assignment authorizes documentation only. Existing
acceptance and release limits retain their recorded scopes.

## Current implementation status

The active toolchain is **LLVM/Clang/LLD 23.1.4 exclusively**; use the
[toolchain requirements](SOURCE_COMPILER.md#active-toolchain). Existing frontend, typed IR, LLVM JIT/AOT
backend, Runtime resource ledger, precise GC and source Task architecture remain
shared. Current API guides determine behavior; proposals record design history.

| Area | Current engineering record |
| --- | --- |
| Language/compiler and entry semantics | [Source compiler](SOURCE_COMPILER.md), [CLI](CLI.md), [language guide](LANGUAGE_BIBLE.md) |
| LLVM migration | [Compatibility](llvm23/COMPATIBILITY.md), [acceptance](llvm23/ACCEPTANCE.md); historical hosted checkpoint is distinct from this repair |
| HTTP | [Current engineering](net/http/CURRENT_ENGINEERING.md), [accepted hosted scope](net/http/ACCEPTANCE.md) |
| VOIDUP A/B | [Phase A](voidup/REPORT.md), [Phase B](voidup/PHASE_B_REPORT.md), [integration](voidup/INTEGRATION_REPORT.md); private Linux engineering retained |
| Current CI repair | [LLVM selector and sanitizer investigation](voidup/INTEGRATION_REPORT.md#llvm-2314-ci-repair); native Windows build evidence remains unavailable and strict libsodium is blocked |
| Performance | [Integrated qualification](compiler-health/INTEGRATED_PERFORMANCE_QUALIFICATION.md), with original incidents and regressions retained |
| Release/installation limits | [Preview blockers](release/PREVIEW_BLOCKERS.md), [installation](install/README.md), [licensing](release/REDISTRIBUTION_AUDIT.md) |
| TCP/UDP direction | [Architecture handoff](net/TCP_UDP_HANDOFF.md); separate implementation release required |

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
Broader SIMD work is not part of this CI/documentation repair. The old FILE200
10 ms aim is not a Boss-mandated release threshold.

## Owner workflow and Stage 7 planning — 19 October 2026

The current role/workflow instructions are maintained once in
[Wrench](WRENCH_CLANKER.md#permanent-workflow-for-every-new-development-stage)
and [Research](RESEARCH_CLANKER.md). The old stage-planning text and original
owner decisions are [archived](https://github.com/Shmellyorc/Void.Language/blob/82e97f541fab7eec760083383569ab9c9ce8f355/docs/DEVELOPMENT_STRATEGY.md#owner-workflow-and-stage-7-planning--19-october-2026).
Boss alone controls Git integration, hosted CI and publication. This document
does not authorize another feature sprint or create a new review requirement.
