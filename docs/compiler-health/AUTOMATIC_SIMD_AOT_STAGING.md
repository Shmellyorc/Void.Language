# AOT-only SIMD staging and Boss CI preparation

6 November 2026. **Boss verdict: RETAIN FOR CROSS-PLATFORM QUALIFICATION.**
This preparation stages the approved AOT-only repair; it does not commit, push,
reset, delete, branch or dispatch CI. Suggested commit title:

**Retain AOT-only target-aware O2 SIMD analysis [skip ci]**

## Exact approved scope

[Literal staging manifest](evidence/automatic-simd/STAGING_MANIFEST.txt):
**160 individual paths**. No directory, wildcard, blanket add or unrelated
local evidence is selected. The [preparation receipt](evidence/automatic-simd/PREPARATION.json)
and [source identity](evidence/automatic-simd/SOURCE_IDENTITY.json) give machine-readable counts/hashes.

| Category | Paths |
|---|---:|
| Production backend | 1 |
| Existing numeric/cache test sources | 2 |
| Required numeric/range/alias/failure fixtures | 6 |
| Current and historical documentation | 8 |
| Curated text evidence and scope metadata | 143 |
| **Approved total** | **160** |

Production SHA-256:
`9725476c2c4001bf3a4d1d775b00893f6d7b5e60f7517839c8f51aa3abfc9525`.
This matches the locally qualified AOT-only candidate. Tests/fixtures match their
recorded qualified hashes; no approved source was changed in this preparation.
The six fixture paths are explicit in the manifest, including the three files
read by the numeric test and the three pure numeric CLI fixtures.

## Review against 83036fac

The only production change is a private optional analysis-machine argument to
`finish`, explicit SLP-off tuning, and passing AOT's existing machine at its two
AOT call sites. AOT lowering/layout/optimization and object emission use the same
existing generic deployment machine; triple, CPU/features, PIC and codegen level
are unchanged. Standard O2 remains one pipeline with pre/post LLVM verification.
Normal non-O2 behavior is unchanged.

JIT implementation/constructor/compile/cache/reload/root-probe source is
byte-identical to accepted baseline `83036fac`. Its calls still select null-target
analysis; ORC is untouched. JIT IR/objects and fixed-embedder cache envelopes were
identical in the recorded qualification. Logical cache fields stay unchanged,
and the existing whole-compiler archive fingerprinting remains intact. This does
not promise identical default cache keys across different compiler builds.

Scalar remainders and descending/empty/dependent behavior remain. Checked arrays
retain bounds, mutation, ownership, precise roots and safepoints. No SIMD syntax,
mandatory vector type, fast-math/FMA permission, unsafe noalias promise, unchecked
raw-buffer access, Runtime or frontend change is present. SLP stays disabled.

The [global JIT+AOT trial](AUTOMATIC_SIMD_QUALIFICATION.md) is historical and rejected;
its cold-start regression and original failures remain recorded. The
[AOT-only qualification](AUTOMATIC_SIMD_AOT_ONLY_QUALIFICATION.md) is retained:
local generic-x86-64 integer controls improved about 2.7×, with explicit compile
and size costs. It does not claim general checked-array SIMD or application-wide
gains. Native Windows/macOS/ARM64 acceptance remains pending Boss-controlled CI.

## Evidence, links and exclusions

The [curated bundle](evidence/automatic-simd/README.md) copies **137 existing text
artifacts**, 2,347,021 bytes, without rewriting their bytes. It includes balanced
raw measurements, all three candidate CTest receipts, original failures,
source/cache proofs, selected optimized IR/disassembly, all eighteen original
numeric fixture sources, FP/Unicode oracles, and complete archive inventories.
The two small historical fixture/parser failures remain visible. Full binaries,
objects, archives, caches and duplicate captures remain local and are identified
by the [bundle manifest](evidence/automatic-simd/BUNDLE_MANIFEST.json).

Report publication changes only current-status banners and links; original report
bytes/hashes remain in the local preparation archive, with before/after hashes in
the receipt. Report links now target committed text evidence or tracked/staged
files, rather than requiring a machine-local `/home/...` hyperlink. Scoped
`.gitattributes` preserves evidence digests across newline-converting checkouts.

Initial snapshot: **13,727 untracked files**. Exactly **9** of those
existing files are approved (six fixtures and three reports); **13,718** are
excluded and preserved. Exclusion classification:

| Existing excluded path group | Files |
|---|---:|
| `docs/collections` | 4675 |
| `experiments/internal-performance` | 2665 |
| `experiments/log-integrated` | 2454 |
| `docs/log` | 1712 |
| `experiments/integration-qualification` | 1102 |
| `experiments/log-sync-path` | 513 |
| `experiments/exact-byte-assembly` | 164 |
| `experiments/byte-assembly` | 157 |
| `experiments/log-v2` | 157 |
| `docs/json` | 90 |
| `experiments/staging-preparation` | 23 |
| `docs/math` | 6 |

The user's concurrent `.gitignore`/unrelated work is outside this allowlist.
These exclusion counts describe the initial snapshot; later ignore changes can
change visible Git counts. No unrelated file/index entry is removed or reset.
The local exact exclusion inventory is identified by hash in the preparation
receipt; no individual review of the thousands of unrelated files is requested.

## Static preparation checks and blockers

No compiler, performance experiment or broad test suite was run for staging.
Static checks cover the approved backend/test/fixture hashes, unchanged critical
JIT/ORC/target/cache/fingerprinting source, raw evidence digests, prospective and
actual staged Markdown links/anchors, exact index scope, text classification and
source/document whitespace. Original CSV/IR-diff whitespace is preserved as data.

**Final staged verification:** 160 approved paths (7 modified,
153 added); all 338 local Markdown targets/anchors resolve in the
actual index. All 137 copied artifact digests and nine approved source/fixture
identities match. All selected blobs are text, and source/fixtures/prose pass
whitespace checks. The largest staged blob is 746,694 bytes.
The full whitespace checker reports 14 original-data notices confined
to literal context lines in the two normalized JSON IR-diff files; their bytes
are preserved rather than rewritten.
No unselected index entry changed during the exact-path add. Your `.gitignore`
work remains outside this scope and is unstaged. All 13,718 initially excluded
files still exist; after the user's ignore changes, only eight remain visible as
untracked in the final review snapshot. Twenty-seven ignored LLVM IR/log proof paths
were force-added individually; no ignore rule or unrelated file was staged.

**Staging blockers:** none. Cross-platform qualification is
not yet complete and is the next Boss-controlled action, not a new release hold.

## Boss CI instructions

After Boss reviews, **commits and pushes**, manually dispatch
[**Compiler link experiment**](../../.github/workflows/ci-link-experiment.yml)
(file **`ci-link-experiment.yml`**), mode **`thinlto-cache`**, on the committed
candidate. Include **all seven** existing jobs:

| Platform | Configuration |
|---|---|
| Linux / Ubuntu 24.04 | Debug |
| Linux / Ubuntu 24.04 | Release |
| Linux / Ubuntu 24.04 | Sanitizers |
| Windows 2022 | Debug |
| Windows 2022 | Release |
| macOS ARM64 | Debug |
| macOS ARM64 | Release |

Use **`[skip ci]` in the commit title** to avoid the separate push-triggered
standard workflow. No workflow/matrix file was changed and no CI was dispatched
by this preparation. Hosted acceptance and all Git commit/push actions remain
with Boss. **Stop for Boss review.**
