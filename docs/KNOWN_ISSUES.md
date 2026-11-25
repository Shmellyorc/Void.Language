# Known issues and implementation boundaries

Current reconciliation: 25 November 2026. The core language is considered complete
for its approved scope. Publishing implementation is temporarily suspended for
repository cleanup. This document authorizes no production change or new feature.

The [language specification](LANGUAGE_BIBLE.md), feature contracts and actual source
supersede dated proposals. Historical “not implemented”, pending-stage and approval
statements are not a current capability checklist. The previous issue ledger is
available in [Git history](https://github.com/Shmellyorc/Void.Language/blob/df33af0a7e8ae75b97d2ab27395a683b69d3994f/docs/KNOWN_ISSUES.md).

## Cleanup corrections and remaining scope

The directory_handle.deinit source-documentation omission is corrected without
changing destructor behavior or weakening the inventory assertion. The maintained
raylib-preparer entry is present; the standard-library documentation inventory
passes. Evidence producers, recipes and collectors now share external output
locations, with corresponding recipe fingerprints and installed helper inputs.
The fresh configure also identified the two required reviewed curl patches;
their original bytes and digests now live in third_party/curl-patches rather than
docs. No language feature or compiler refactor is part of this cleanup.

Historical capability claims have been reconciled with the current feature
contracts and [owner decisions](DECISIONS.md). Old proposals and acceptance gates
are retained in Git history. The following limitations and unresolved incidents
remain; passing cleanup checks does not resolve them.

## Unresolved incident evidence

Passing later tests, changing LLVM versions or moving evidence does not prove a
causal repair. These are separate incident families, not one undiagnosed defect.
The [compiler-health inventory](compiler-health/ISSUES.md#historical-unresolved-observations)
retains the original descriptions and reproduction limitations.

| ID | Documented observation | Current classification and disposition |
| --- | --- | --- |
| H1 | LLVM 19.1.7 DSE SIGSEGV in `CallBase::getMemoryEffects` | Insufficient evidence for original cause or repair. LLVM 23.1.4 success is separate evidence. No new crash campaign is authorized. |
| H2 | Release async filesystem runner SIGSEGV | Insufficient original fault PC/core, exact temporary source and binary identity. Reconstructed workloads do not establish the original cause. |
| H3 | Stage 9 preliminary Debug async-emitter and numeric process faults | Nonreproducing historical observations; later passing controls do not establish a repair. |
| H4 | Pinned Clang BDCE/DemandedBits build crash compiling `conversion.cpp` | Historical compiler-build incident; distinct from source-program execution. |
| H5 | LLD optimizer diagnostic stacks, including GVN/GlobalISel observations | Preserve individual diagnostics and actual enclosing exit status; no inferred failure or common cause. |
| H6 | Parallel ThinLTO temporary-object rename/link failure | Serial retry passed; no causal repair established. Distinct from VOID's project cache. |
| H7 | Stage 10 Step 37 publication/default-root anomalies | Did not reproduce in controlled frozen-library reruns; preserve uncertainty. |
| LLVM23-Windows | Earlier intermittent Windows optimizer failure | Later accepted hosted scope did not reproduce it. Current risk disposition remains with Golden Wrench/Boss. |

Original tracked records remain recoverable from Git. Local records removed from
the checkout were moved intact under
`~/Documents/Void.Language-archives/repository-cleanup-2026-11-25/retained-repository/`.
Their exact original paths and destinations are in `tracked-removals.json` there.

## Current intentional limits and partial implementations

| Area | Current behavior | Evidence and recommended disposition |
| --- | --- | --- |
| Async ownership | Completed frame/result roots and Work retire after quiescence. GC checkpoints can reclaim unobservable completed native Task/group records; observable results, pending dependencies/failures and active owners remain. No whole-process constant-memory guarantee. | `src/runtime/async.cpp`, [retention contract](STAGE8_ASYNC_RETENTION.md), async lifecycle tests. Preserve the ownership model; broader retirement needs an approved design. |
| GC and performance | Coordinated precise nonmoving GC; finite compiler budgets and conservative authority remain. Later root-lifetime improvements supersede blanket function-exit-only claims. No certified pause/throughput bound. | Runtime/LLVM rooting source, [GC](GC_RUNTIME.md), [root lifetimes](compiler-health/G39_MANAGED_REFERENCE_LIFETIMES.md). Measure a current workload before proposing changes. |
| Cache | G95 retains ten validated recent entries. Unknown/corrupt/interrupted files remain outside automatic deletion authority; no byte quota. Windows disk persistence safely remains unavailable. | `src/backend/llvm/native_cache.cpp`, `cache_retention_processes`, [current cache policy](compiler-health/stabilization/CACHE.md). Old “no eviction” claims are obsolete. |
| Reload and editor analysis | Explicit compatible generation replacement exists. Editor watched-file refresh is not an automatic live application reload loop. Full incremental compilation is absent. | Reload/session source, `src/lsp/main.cpp`, [G95 reload audit](projects/G95_VOIDC_PERSISTENT_CACHE.md#reload-audit-and-demonstrated-scope). Do not advertise automatic hot reload. |
| Rename and clients | Rename requires version-safe edits and supported ordinary function/global/constant/uncaptured-local identities. Traditional Vim intentionally declines unsafe rename. | `src/lsp/main.cpp`, tooling/editors regressions, [editor contract](EDITOR_INTEGRATION.md). Broader rename categories require a separately reviewed contract. |
| Constant evaluation | G93 pure compile functions have finite work/stack limits and a bounded operation set. Module-time pattern evaluation and unsupported text/collection constant APIs still diagnose. | `src/frontend/compiler.cpp`, [G93](projects/G93_COMPILE_FUNCTIONS.md), `compile_functions_source`/`compile_functions_lsp`. No general interpreter is implied. |
| Object/interface model | Managed structural views and generic interfaces exist. Standalone value boxing, broad downcasts/interface conversions, interface inheritance/default bodies, method-only reuse and transitive promotion remain design directions. | Interface/composition contracts and source regressions. Do not treat historical proposals as new authorization or abandon them silently. |
| Native/parallel authority | Typed C imports and managed native packages exist. General managed-reference sharing across workers, arbitrary callable/task transfers, arbitrary managed public ABI and open-world providers are not provided. | Native/async contracts, frontend and independent IR validators. Preserve admitted signatures, ownership, effects and generation leases. |
| Syntax and data boundaries | ASCII identifier/module policy, explicit slice endpoints, exact numeric types, bounded dictionary-key families, no general custom hash protocol and no value-struct `deinit` remain explicit limits. Inline one-line bodies, weak references, detached tasks, generators and async destructors are not implemented. | [specification](LANGUAGE_BIBLE.md#deferred-design), numeric/dictionary/initialization contracts. Future disposition belongs to Boss. |
| Standard library | Async atomic save, general deadline composition/async queues, broader archive writer formats, other-host process capture and Schannel supplied-P12 identity are outside current bounded APIs. | IO/process/archive/async/HTTP contracts and their existing tests. These are separate API/platform boundaries, not evidence that the core language is incomplete. |
| Crash collection | Opt-in compiler/server supervision exists. Generated AOT applications are outside G90; Windows unmarked native exception capture is deliberately unsupported. | [G90](projects/G90_CRASH_DIAGNOSTICS.md), `src/diagnostics/crash_diagnostics.cpp` and crash tests. Platform implementation and native qualification are distinct. |

## Qualification and release decisions

- Historical editor/HTTP timing regressions and logging pause/allocation limits
  require their recorded disposition. FILE200's old 10 ms aim is advisory by Boss
  decision; it is not an automatic release blocker. A fresh current-build timing
  result is not established by this cleanup.
- Native Windows/macOS qualification is feature- and revision-specific. Older
  installation/HTTP successes do not qualify later compiler, editor or package
  changes; older pending statements do not erase newer scoped results.
- Public distribution trust, production key enrollment, exact payload licensing,
  corresponding-source/replacement obligations, platform closures and release
  channel decisions remain separate from language completeness. See
  [release blockers](release/PREVIEW_BLOCKERS.md),
  [production trust](voidup/PRODUCTION_TRUST.md) and the later native qualification
  records. No publication or resumed publishing implementation is authorized.
- Vendored curl's `KNOWN_BUGS.md` and `KNOWN_RISKS.md` describe upstream material.
  Disabled protocols/CLI-only behavior are not automatically VOID defects. Relevant
  HTTP/Schannel observations need applicability and reproduction evidence before a
  VOID defect or repair is claimed. No vendor behavior was changed by this review.
