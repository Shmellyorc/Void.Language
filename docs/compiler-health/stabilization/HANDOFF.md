# Golden Wrench handoff — compiler stabilization

27 October 2026. Baseline `1d4ad15f5215838d209e48bb47ac7c3f52b05371`.
Implementation, local correctness gates and controlled performance audit are
complete. **STOP: ready for independent Golden Wrench source/evidence review.**
No independent or hosted acceptance is claimed.

**STATUS: NEW STANDARD-LIBRARY DEVELOPMENT PAUSED — COMPILER STABILIZATION IN PROGRESS.**
The hold remains until independent review and Boss authorization to resume.

## Delivered behavior

Ordinary `voidc run` performs full checking and optimized compilation on a miss,
then automatically reuses the existing validated `.void/cache/v1/` native object.
`--no-opt` has a separate object, and `--no-cache` bypasses persistence. Legacy
`--cache` retains its no-IR-O2 behavior and status line. Cache identity covers
source/imports, compiler/Runtime link inputs, target/features/data layout,
optimization/codegen, entry/emission and contracts. Security/admission/atomic
publication boundaries remain. The [cache policy](CACHE.md) records disposition,
corruption behavior and platform fallback.

Shared frontend hints now project results/tasks correctly, infer nested
collections/choices and avoid exponential arithmetic revisits. Exact
`identity(read()?)` and `json.stringify(json.parse_as<i32>(text)?)?` forms work.
Order, exactly-once evaluation, propagation/defer/readonly and LSP behavior have
regressions. Rejected deep receiver chains now reach the existing Debug guard
on 1 MiB through smaller recursive frames, with unchanged diagnostics/recovery.

Existing JSON-generated code shares repeated admission/find/descent/conversion
work. Pointer homes and immutable descriptor initialization are compact; compound
root geometry, every required word/safepoint, value copying, budgets, error order,
paths, offsets and precision remain. Full checking/typed validation precedes
conservative CLI entry emission. Eager globals, callbacks, dispatch, constructors,
cleanup and async targets remain reachable; embedding and raw `emit` retain all
exports by default. C++ host clients must rebuild against the changed constructor/
lower signatures. Runtime source, header and Release archive are unchanged.

## Measured result

Same preserved bytes, pinned LLVM 19.1.7, same native Default codegen, actual
1 MiB stacks and fresh processes; uncached final runs explicitly bypass persistence.

| Work | Frozen baseline replay | Final |
|---|---:|---:|
| DTO128 optimized uncached | 297.021 s | 8.149–8.609 s across six serial runs |
| DTO128 optimized peak RSS | 1,883,968 KiB | approximately 135 MiB |
| Unused JSON import optimized uncached | 10.307 s | 0.227 s |
| Real JSON save example optimized uncached | 39.750 s | 6.255 s |
| Left arithmetic 20 check, median | 1.518449 s | 0.008038 s |
| DTO128 standalone IR O2 | 107.308 s | 2.795 s |
| DTO128 standalone native codegen | 180.651 s | 4.835 s |
| Largest DTO128 native function frame | 414,344 bytes | 13,208 bytes |

The original research DTO128 result was 309.755 seconds / 1,913,040 KiB. The local
under-10-second objective is met without cache hits or weaker optimization.
DTO128 warm optimized cache is about 0.468 seconds; tiny warm startup about
9 ms. DTO32 real source edit/run is 4.239 seconds, then 0.149 warm. Full width
8/16/32/64/128, both IR policies, application, cold/warm/AOT and RSS/CPU tables
are in [measurements](BENCHMARKS.md), with sample counts and limitations.

The [audit](AUDIT.md) attributes the old costs to oversized generated IR/root
scaffolding. Largest post-O2 block falls 101,915→1,572; maximum descriptors
9,603→173 and registered reference words 10,646→826. LLVM pass timings,
preserved IR, machine counts/sizes and ORC boundaries are recorded. The cold
ORC machine-generation window is about 4.086 seconds, linking/lookup about
2.7 ms. Runtime lookup/allocation/poll/collection, heap drain, async retention
and uncached compiler lifetime cycles are measured separately.

## Verification and evidence

Fresh selected-target builds and frozen-input acceptance passed:

- Release **94/94**.
- Debug **94/94**.
- Strict ASan/UBSan **93/93**, with leak checking enabled; one existing LSP stack
  probe is excluded in this configuration and passes in the other profiles.

All receipts confirm unchanged before/after source/build hashes. The preserved
DTO ladder additionally passes JIT and independently linked AOT in both IR modes,
all three profiles, on 1 MiB stacks. Extra entry-policy checks pass 72 dynamic
JIT/AOT executions, 36 builds and six baseline executions, plus 48 async
all/entry checks. Cache evidence includes actual Release→Debug compiler changes,
source/import edits, separate projects, fresh application state, unsafe/corrupt/
interrupted/concurrent cases and reload/ownership/failure recovery. No timeout,
stack limit, validator or LLVM pass was weakened.

[Verification](VERIFICATION.md), [changed-file inventory](CHANGES.md),
[engineering ledger](IMPLEMENTATION.md), [incident ledger](INCIDENTS.md),
[portable evidence guide](evidence/README.md) and [manifest](evidence/manifest.json)
provide exact source/build/helper/tool hashes, commands, flags, logs and artifacts.
The manifest indexes 571 measurements, all retained failed attempts, 892 source
hashes and 26 frozen local compiler/failure images. The original research archives
and first-failure evidence remain intact.

| Final CLI image | SHA-256 |
|---|---|
| Release | `fc2fa48b34678e4082456cddad78e0fd62d60934673a409d23a301ad4525f1f2` |
| Debug | `05ebd7496bfeea13e06dd31ec3082d921003cd93dfbaf1fb655774a3bf604479` |
| Strict sanitizer | `b853f4441cca06f7907ebefd8de115d29e5ab1b54ae55f4b7dd4c540feb12a52` |

## Tradeoffs, failed experiments and remaining issues

A broad compound-root packing experiment failed unchanged independent proof
observers and was narrowed to pointer-shaped homes. Both intermediate hint-frame
reductions still overflowed and are preserved. Stale target/provenance/test-probe
errors and measurement helper alignment/RTTI flag mismatches are classified with
first sources/images/logs. Their corrected results are explicit; no passing retry
is offered as a fix for an unexplained historical crash.

Descriptor templates add 41,792 bytes of `.data.rel.ro` to DTO128 while `.text`
drops 1,443,520 bytes. Tiny first cached startup has about 13 ms publication
premium over uncached in this series. Balanced arithmetic stays at process-floor
scale with overlapping ranges. Full timings and current Debug/sanitizer costs
remain visible, including slower instrumented execution.

Runtime root lookup remains linear; layout/access allocation churn, fresh DOM
construction, conservative roots and root-duration task/result records remain
measurable boundaries. The existing retention contract drains cleanly. A glibc
allocator-cache control explains tiny cycle allocator growth; this is a bounded
observation, not a universal leak proof. Entry emission conservatively retains
dispatch families/full metadata. Native-cache eviction, incremental editor
compilation and verified Windows persistence are not introduced.

**H1–H7 remain unresolved historical observations**, separately classified in the
[active inventory](../ISSUES.md). No new cause or repair is invented for LLVM DSE,
Release async I/O, Stage 9, pinned Clang or linker/cache incidents. These Linux
focused results do not claim native Windows/macOS or hosted CI acceptance.

## Owner boundary

Review the shared hint/type boundary, JSON work/error ordering, entry graph roots,
root geometry/initialization, optimized cache admission/identity and the frozen
source/build/measurement correspondence. The source overlay plus baseline archive
and exact helper recipes support independent replay. Keep the local frozen tree
until review finishes.

No Git staging, commits, branches, pushes or hosted CI were performed. HEAD remains
at the baseline. Protected root README SHA-256 remains
`c287f35842b03a1c5ac170718e674cc5498f877ade76eede5273859921d62add`.
No ptrace-based debugger or tracing was used. Independent review and Boss own
acceptance, Git/CI actions and any standard-library resumption.
