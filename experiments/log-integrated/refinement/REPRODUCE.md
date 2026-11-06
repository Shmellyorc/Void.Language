# Integrated candidate final refinement

> Publication note: this commit keeps selected text evidence. Full local archives,
> binaries, candidate trees and disposable runs remain excluded and preserved;
> see the [artifact scope and original-document hashes](../../../docs/compiler-health/evidence/integrated-performance/LOCAL_ARTIFACTS.md).
> Measurements and conclusions below are unchanged; artifact links are portable.

The current editable source remains `../candidate`. Production V1 is unchanged. The complete final delta is [refined-candidate.patch](refined-candidate.patch); the two-file delta against the first integrated candidate is [refinement.patch](refinement.patch). The original integrated source is preserved in [original-integrated-source.tar.gz](../../../docs/compiler-health/evidence/integrated-performance/LOCAL_ARTIFACTS.md#local-artifact-21), and all previous evidence under `../evidence` is protected unchanged.

Read the [final report](../../../docs/log/INTEGRATED_LOGGING_REFINEMENT_RESULT.md), [source proof](evidence/refinement-source-proof.json), [source digest](evidence/candidate-source-identity.json), [validation results](evidence/final-qualification.json) and [raw measurement summary](evidence/summary.json).

`bootstrap.py` redirects imported existing experiment helpers to **this round's evidence directory**. `run-driver.py` applies that redirection to subprocess drivers as well. It deliberately leaves the original driver files and historical evidence unchanged. Do not execute the parent drivers directly for a new refinement round: their default output directory is the historical parent evidence.

The local selective build uses the same retained LLVM/Clang/LLD 23.1.4 Release, Debug Runtime and strict ASan/UBSan recipes as the first integrated experiment. Existing private libraries are copied, never hardlinked, before archive replacement. Only the frontend changed in the final implementation; unchanged instrumented Runtime/backend/IR translation units and measurement objects are reused. The final compiler source identity is stamped into a new private cache-identity archive before final linking. Normal/O2 AOT objects are freshly emitted and independently linked. Debug Runtime tests share the Release frontend/backend and one authority for format/date kernels.

Actual chronological work is recorded in [commands.jsonl](evidence/commands.jsonl), with stdout/stderr per label:

1. `prepare.py` verifies the first candidate, snapshots its source, and protects previous work. It must not be rerun in this completed directory.
2. `initialize.py` copies private archives, objects and source fixtures into this round. `implement.py` and `focus.py` record the discarded native-staging formatter attempt. Its source, archive, object, identity, native test and all focus samples are retained.
3. `fallback.py` restores that Runtime change completely, removes automatic frontend fusion, and repairs the isolated advanced test using semantic targets observed in each failing execution.
4. `build-final.py`, `complete-hosts.py`, `link-measurement.py`, and `inspect-fallback.py` compile/link the final source, original budget-1 correctness hosts, fresh AOT objects, baseline/candidate timing hosts and IR proof. Timing hosts retain budget 256.
5. `validate.py` runs the final host matrix, original/adapted advanced tests, I/O ownership and three-way cache compatibility. All 17 hosts passed. Its supplemental IR assertion originally overlooked the timestamp-prefix join; that failed binary/source and logs are retained. `finish-validation.py` corrected that test-only count to two ordinary joins and one encode, then completed IR and the original byte/UTC oracles. No candidate code changed and the passing host matrix was not repeated.
6. `measure.py` waited for the full validation gate, then ran parent `performance.py plain`, `performance.py profile`, and `heavy.py measure` through the redirected driver. All 96 primary, 12 instrumented and 224 heavy rows are retained. `summarize.py` aggregates all samples without trimming or pooling execution kinds.
7. `framing-control.py` records one separately declared fixed-core diagnostic round for remaining near-limit framing variation. Its plan, all 112 rows including warmups, and summary are separate from the primary results.
8. `report.py` renders the final report; `verify.py` checks protected content, source/binary identities, output/count/resource invariants, report links and the local artifact manifest.

These are local evidence drivers, not an idempotent or portable one-command build system. Do not rerun mutators on the finished source. For source reconstruction, apply the full final patch to a fresh private copy of V1, or apply the incremental patch to the archived original candidate. For binaries, replay the recorded successful final recipes in dependency order with fresh private output/work directories and the same toolchain/dependency closure. Preserve failed/slow results and all original evidence. Candidate supporting directories include symlinks to repository resources; treat them as read-only.

No production replacement, staging, commits, pushes, branches, hosted CI or root README modification is part of this refinement.
