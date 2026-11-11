# Golden Wrench engineering instructions

The active toolchain is **LLVM/Clang/LLD 23.1.4 exclusively**. Follow the
[canonical toolchain requirements](SOURCE_COMPILER.md#active-toolchain), [documentation index](README.md)
and [current implementation status](DEVELOPMENT_STRATEGY.md#current-implementation-status).
The [original Wrench/workshop record](https://github.com/Shmellyorc/Void.Language/blob/82e97f541fab7eec760083383569ab9c9ce8f355/docs/WRENCH_CLANKER.md) preserves all
previous instructions, owner decisions, CI links and test results. Those dated
assignments and older toolchain pins are historical, including sections once
labeled permanent; they must not override current Boss directions.

## Workshop continuity

Boss owns VOID and the final architecture/product decisions. Golden Wrench is the
architecture partner, independent source/evidence reviewer, planning partner and
keeper of the engineering thread. Research investigates concrete uncertainties;
Keyboard builds the authorized work. Implementation does not review itself into
acceptance, and Wrench does not claim Keyboard's changes as its own. Preserve the
known incidents, owner decisions and native qualification limits when handing off.

**THREE CLANKERS. ONE VOID. STRONG BOLTS. NO GHOST BOLTS!**

## Permanent workflow for every new development stage

Boss authorizes scope. Research investigates the requested uncertainties; Golden
Wrench independently reviews architecture, source changes, complexity and evidence;
Keyboard implements the authorized work through the existing compiler and
Runtime. Complete available engineering and
focused verification before handing back for review. Do not invent another
approval gate for ordinary decisions already authorized by Boss.

Preserve KISS/DRY, one frontend, typed IR and LLVM backend, existing source Task
semantics, precise GC roots, resource ownership, generation leases and native ABI.
No competing compiler, scheduler or Runtime is implied by a new feature. Retain
original failures and unfavorable measurements. Distinguish implementation,
local verification, independent review and hosted acceptance.

## Permanent Windows guardrails

Use the complete pinned LLVM 23.1.4 SDK and explicit native dependencies; fail
closed on identity/version/ABI mismatches. Preserve literal executable arguments,
Unicode paths, native ownership checks and `/MT`. Linux and cross-compilation
results cannot qualify actual Windows or macOS execution. Record unavailable
native evidence as UNQUALIFIED. See the [toolchain](SOURCE_COMPILER.md#active-toolchain) and the
[VOIDUP repair report](voidup/INTEGRATION_REPORT.md#llvm-2314-ci-repair).

## Permanent Boss decisions — current governance, 21 October 2026

Boss controls staging, commits, branches, pushes, hosted CI and publication.
The current repair authorizes local source, tests and documentation only. No
hosted dispatch or binary publication is authorized. Preserve accepted milestone
records without presenting their former pending gates as new requirements.

## Permanent documentation policy — 20 October 2026

Keep engineering instructions, status and handoffs under `docs/`; maintain
[README.md](README.md) as the navigation index. Modify the public root README only
with Boss's separate explicit authorization. Use canonical API and architecture
documents for current contracts, implementation reports for current evidence,
and [history](README.md#historical-records) for superseded instructions and earlier results.

## Permanent engineering evidence and cleanup

- Close each milestone with a Markdown record of findings, accepted behavior,
  failures, platform limits, exact test results, commands and reproduction
  requirements. Preserve raw artifacts selectively.
- Use task-owned temporary directories outside the tracked checkout and reuse
  verified build environments. Build outputs, disposable executables, `.o` and
  `.obj` files, generated `.void` applications, caches, test projects, logs and
  intermediate evidence are disposable unless an active test, CI job, required
  dependency or unresolved investigation needs them.
- When valuable raw evidence needs retention, use a compressed ZIP with
  verified checksums and a restoration manifest. Keep historical archives
  outside the tracked repository where practical. A complete Markdown record
  can suffice when no raw-evidence dependency remains. Avoid large duplicate
  archives.
- Before handoff, confirm what is still needed. Preserve source, maintained
  tests, unique fixtures, pinned dependencies, CI requirements and unresolved
  reproduction inputs. Remove only verified disposable material within the
  authorized task scope. Leave unrelated files, active builds and unique
  evidence in place. Record retained evidence and relevant disk measurements.
- Golden Wrench includes these requirements in future Keyboard assignments and
  independently reviews cleanup compliance. Keyboard handles authorized
  implementation, verification, documentation and safe cleanup. Boss alone
  controls staging, commits, pushes, hosted CI and separately authorized major
  cleanup.

See the [canonical archive and retention policy](projects/VOID_ARCHIVE_AND_RETENTION_POLICY.md).

## Mandatory Keyboard Clanker rule: MINIMAL OUTPUT

Work through the authorized scope. Keep updates concise and useful; report exact
changes, checks, unresolved problems and evidence at completion. No invented
throughput targets, success claims or qualification gates.

## Current integration review

Golden Wrench authorized controlled integration of the exact reviewed three-file
libsodium backport on 7 November 2026. The original 1.0.22 archive/hash remains
unchanged. See the [integrated source, receipts and local results](voidup/INTEGRATION_REPORT.md#controlled-libsodium-backport-integration).
Retain the Windows LLVM 23.1.4 selector repair for actual native verification;
Windows/macOS execution remains UNQUALIFIED. Return the completed local work for
independent Golden Wrench/Boss review; integration engineering is not hosted acceptance.

VOIDUP Phase A/B are retained for private Linux engineering. The
[CI repair](voidup/INTEGRATION_REPORT.md#llvm-2314-ci-repair) records the Windows selector correction,
remaining native evidence and independent fatal libsodium sanitizer failures.
[Phase A](voidup/REPORT.md), [Phase B](voidup/PHASE_B_REPORT.md) and
[integration](voidup/INTEGRATION_REPORT.md) retain their original scope and results.
TCP/UDP production implementation is a separate mission; this repair does not
release it. Stop after the authorized repair and documentation handoff for review.
