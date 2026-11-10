# Slice 2 final correctness closeout

8 November 2026. HEAD remains `2f667ea338127b8f91c3136804377ef3fa5218d6`.
Local qualification is complete; **Golden Wrench final S2 review remains pending.
Stop before S3. Hosted CI remains on strict hold.**

The [original S2 report](SLICE2.md), [S1 closeout](SLICE1_CLOSEOUT.md), all original
receipts and raw evidence are retained byte-for-byte. This review changes exactly
three source files; 160 of the 163 starting inventory files remain byte-for-byte
unchanged. New report/evidence files are separately inventoried.

## Tombstone proof

The existing `SourceGraph::resolve` guard in
[compiler.cpp](../../src/frontend/compiler.cpp#L5206) checks
`overlay != overrides.end() && !overlay->second` and throws the owned `VOID3002`
"Source is explicitly unavailable" diagnostic. It runs before physical traversal,
reader invocation and either unit-cache lookup. No source-reader repair was needed;
a comment records this invariant.

- `parse` resolves before its sole `parse_resolved` call. `load` also resolves
  before its unit-cache lookup, depth guard and `parse_resolved` call. These are
  the only calls to `parse_resolved`; its optional dereference and `graph.reader`
  callback therefore require a present overlay value.
- In [ProjectCapture](../../src/frontend/project_selection.inc), the reader calls
  `capture`; D inventory skips tombstones, then resolves before direct capture.
  `recapture_source` resolves before capture. A capture cache hit cannot introduce
  a deleted source because the same frozen source map governs the entire call.
- The selected TOML optional is separately guarded by `boundary("")`: a root
  manifest tombstone fails admission before the constructor dereferences it.

Regressions cover explicit overrides and manifest-authored entries, automatic
missing-main behavior, dirty imported modules, excluded-but-imported E sources,
physical files hidden by tombstones, cyclic imports, bare-file legacy roots,
later overlay revisions and owned diagnostics after disk/overlay removal. Valid
physical bytes never substitute for a tombstone. These synchronous APIs consume
immutable overlay revisions; concurrently mutating a C++ map is not a supported
snapshot operation. Disk changes during original capture/recapture are tested
separately below.

## Recapture correction

The old blanket `catch (const std::exception&)` converted validation and permission
failures to `VOID5011`. The retained original reproduction expected `VOID5009` for
a same-name source replaced by a symlink but received `VOID5011`.

The fresh capture now observes the original capture through a call-local const
pointer. It compares successfully observed root identity, directory membership,
boundaries, raw manifest bytes and local source bytes before downstream validation
can reinterpret a changed snapshot. Explicit final E-byte/map comparisons remain.
Observed mismatches produce owned `VOID5011`; failed admission/read operations keep
their original diagnostic. The generic exception conversion and mismatch-as-
`invalid_argument` exceptions are removed. No parser, semantic, VFS authority,
source limit, dependency pin, LLVM path or application behavior was replaced.

Eight Linux notification-driven regressions mutate files after the entry and an
E-only helper are loaded, while an excluded E source is being read:

| Observation/failure | Required and observed result |
|---|---|
| Changed D bytes, changed E-only bytes | `VOID5011` |
| Manifest replaced by malformed `[=roject]` | `VOID5011`, before schema parsing |
| Deleted source / changed membership | `VOID5011` |
| Same-name symlink replacement | `VOID5009` admission error |
| Source or manifest becomes unreadable | `VOID3002` read error |
| Source directory becomes inaccessible | `VOID5009` enumeration error |

The pre-existing concurrent writer and nested-boundary regressions remain enabled.
No failed or incomplete verification publishes a successful project. This is
bounded recapture/detection, not an atomic filesystem snapshot; a validation
failure without a completed differing observation is not proof of a byte change.

## Executed final qualification

| Configuration | Selected local qualification |
|---|---|
| Linux Release | 36/36 CTests; 476 selection assertions |
| Linux Debug | 36/36 CTests; 476 selection assertions |
| Linux fatal ASan/UBSan, RelWithDebInfo | 36/36 CTests; 476 selection assertions |
| Windows 10 x64 build 19045 Release | 353 selection assertions; 88 project, 11 nested-project and 17 legacy checks |
| Windows 10 x64 build 19045 Debug | 353 selection assertions; 88 project, 11 nested-project and 17 legacy checks |

Linux totals 108/108 selected CTest outcomes, not the entire repository suite.
Each Linux profile includes the 88 project CLI processes, 11 nested-project
processes and 17 manifest/legacy checks. Actual normal/O2 JIT and independent AOT,
imports, D/E initializer isolation, argument forwarding, cache invalidation,
standalone execution, library checks, negative admission, ownership, depth/bounds,
frontend/tooling, numeric/generic and globals regressions pass. Exact commands,
per-test output, binaries/input hashes and raw logs are in the receipt below.

Strict configuration retains `VOID_SANITIZERS=ON`, address+undefined instrumentation,
`ASAN_OPTIONS=detect_leaks=1:halt_on_error=1:exitcode=87` and
`UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1:exitcode=86`.

Windows builds recompile the changed frontend, manifest, main and selection
translation units, generate fresh cache identities, and link inventoried unchanged
backend/Runtime/provider objects. Native HTTP/host dependency verification and
manifest/resource admission remain enabled. Both profiles use the final 126-file
source packet plus the hash-checked shared nested-project test script. This is
incremental native qualification, not a full clean CMake build. The original nested
invocation failed because that script was omitted from the packet; a corrective supplement executes it against both unchanged native binaries.

New failure history is retained: (1) the first legacy test used the project-root
overlay identity for a bare-file adapter; corrected to its entry-parent identity;
(2) the first mutation fixture deleted the entry before E load and correctly got
`VOID3002`; the notifier was moved to an excluded E source to target recapture;
(3) the corrected original-source reproduction exposed the blanket-catch defect;
(4) the native harness omitted nested.py from its packet; its original exit 2 and
FAIL receipt remain intact, followed by passing shared-script reruns on both native
binaries. Final corrected executions above pass. No tests were disabled or weakened.
Unchanged vendored TOML literal-operator deprecation warnings remain visible in
native compile logs.

Remaining limits: Windows symlink/reparse fixtures lack guest privilege; Linux
exercises those negative cases. The new notification/permission recapture tests
are Linux-only. Windows persistent native-cache warm hits remain unavailable.
Windows fatal sanitizers, Windows Server 2022, macOS and a full clean Windows build
were not executed. Existing project editor-protocol/live-reload limitations in
the original S2 report remain; S3, application metadata, FFI and raylib are untouched.

## Exact source and Git inventory

- [Review-only source patch](evidence/slice2-final-review-source.patch): three files,
  against the accepted starting S2 tree.
- [Current combined baseline source patch](evidence/slice2-final-baseline-source.patch):
  all 41 source/architecture paths against HEAD, including S1 and S2.
- [Final receipt](evidence/slice2-final-review.json): source, binary, preserved-input
  and raw-evidence hashes, executed commands, results and explicit limitations.
- [Complete layered Git inventory](evidence/slice2-final-review-inventory.txt): every
  modified/untracked path, its original S1/S2 layer and this review's additions.

Both patches pass reverse checks and exact forward reconstruction in temporary
directories. Reports/evidence are excluded from source patches to prevent recursive
patch contents, and are covered by the full inventory and receipts. HEAD and index
are unchanged; nothing was staged, committed, pushed, dispatched or published.
Security settings and dependency pins remain unchanged. Stop for Golden Wrench.

Final expanded Git inventory: 247 paths (25 modified, 222 untracked);
84 new review report/evidence paths. The complete per-path inventory above
distinguishes every original S1/S2 layer.
