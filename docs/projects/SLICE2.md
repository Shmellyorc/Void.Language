# Slice 2: compiler-owned project discovery and entry selection

8 November 2026. Unchanged baseline/HEAD:
`2f667ea338127b8f91c3136804377ef3fa5218d6`.

Golden Wrench accepted [Slice 1 closeout](SLICE1_CLOSEOUT.md) for local continuation.
Boss authorized this discovery/selection slice. The reviewed 83-path Slice 1 tree
was checkpointed before editing; its implementation, fifteen-document architecture
work, parser pin/patch/license and original evidence are preserved beneath this
layer. Source changes and qualification are local. **S2 independent review remains
pending; stop before S3.** No staging, commit, push, hosted dispatch, dependency-pin
change, security-policy relaxation or binary publication.

Linux final qualification passes 36/36 selected CTests per profile (35 broad regressions
plus the deliberately selected child-project fixture), 108/108 outcomes across
Release, Debug and fatal strict. Native Windows 10 Release/Debug execution is
qualified below with its incremental-build and platform limits.

Review artifacts:

- [Exact Slice 2 source delta](evidence/slice2-source.patch), against the reviewed
  starting working tree rather than treating Slice 1 as new Slice 2 work.
- [Combined baseline source diff](evidence/slice2-baseline-source.patch), including
  the preserved Slice 1 implementation and architecture changes against HEAD.
- [Receipt](evidence/slice2-local.json), with exact input/binary/evidence hashes,
  test commands/results, failures, corrections and platform qualifications.
- [Complete layered Git inventory](evidence/slice2-inventory.txt), distinguishing
  preserved Slice 1 paths, Slice 1 paths amended by S2, and new S2 paths.

Source patches exclude `docs/projects/` reports/evidence to avoid recursive patches
containing themselves. Those files are separately inventoried and hashed. Patches
are checked in reverse and reconstructed forward without touching the Git index.

## Delivered behavior and architecture

`analyze_manifest_project(root, entry_override, sources, manifests)` in
[frontend.hpp](../../include/void/frontend.hpp) returns a shared immutable
ProjectAnalysis only after successful manifest, admission, discovery, semantic and
snapshot-stability gates. It retains the canonical physical root, optional selected
entry, existing application ValidatedAnalysis, separately owned D/E source
snapshots, immutable Slice 1 Manifest and a versioned selection key. Valid metadata
can accompany a failed result; no failed result supplies ProjectAnalysis or IR.

The implementation is [project_selection.inc](../../src/frontend/project_selection.inc)
inside the existing compiler frontend. SourceGraph separates `parse`/`parse_resolved`
from import expansion in `load`. Each discovery Unit passes through the same lexer
and Parser once; load moves captured Units into E as needed. Import-depth admission
still precedes parsing a newly expanded source. There is no second language parser,
editor resolver, LLVM lowering path or Runtime application-context implementation.

For applications, automatic selection scans eligible sources, recognizes ordinary
top-level synchronous nongeneric parameterless main with no result or exact i32,
and rejects wrong free-main signatures. Visibility does not change eligibility.
Methods, nested functions and unrelated declarations do not become candidates.
Zero candidates diagnoses even for project check. Multiple candidates identify
every considered main, sorted by canonical identity/offset; large lists summarize
the full count and explicitly truncate displayed related locations at 128.
`main.void` receives no priority. A unique candidate's later type/body error fails;
there is no search for a different executable.

Only E receives application semantic validation, typed IR, lowering and eager
global initialization. Unimported D helpers may have semantic errors or global
initializer effects without entering E. Parse errors anywhere in D still reject
automatic selection. Importing a helper makes its existing semantic/startup rules
apply. An imported library or excluded helper is not promoted into D by import;
an independently eligible D file stays a candidate even when another seed imports it.

The manifest fixes VFS `/`; the selected source fixes root declaration ownership.
Deep entries such as `/src/features/launcher.void` retain their module identity and
own public/internal root declarations. The manifest is not a source module. Existing
module/stdlib identities, visibility, globals, closed native bindings, independently
validated typed IR and normal/O2 JIT/AOT pipelines remain in use.

Libraries check the union of eligible sources and their import closures with an
explicit declarationless root environment. They need no main and may have an empty
source set. Ordinary helpers named main are permitted. No arbitrary library file
gets root declaration privileges; executable top-level statements reject. Successful
library snapshots contain no application entry or executable ValidatedAnalysis.
Project run/build/emit reports unsupported library operation; standalone explicit
file commands remain available separately.

## Selection, physical admission and immutable capture

CLI `--project DIR` pins exactly that directory's manifest and conflicts with a
positional source/directory selector. A directory operand, or run/build cwd, opts
in when its exact manifest is present; wrong-case/aliased/invalid manifests fail
closed. No ancestor search is added. Bare explicit `.void` files keep their entry-
parent VFS root and ignore adjacent manifests. Manifestless discovery still calls
the original `discover_project` helper.

`project.entry` or manifest-relative `--entry PATH` bypasses D uniqueness scanning,
then loads only E under existing main/script/legacy/declarations-only classification.
The CLI override wins over the authored override. Invalid overrides never fall
back. Root/source policy is still admitted; nested-project crossings reject.
Library `--entry` is a usage conflict. Default project native output is
`<manifest-root>/build/<entry-stem>[.exe]`; explicit output/publication rules remain
the existing native builder's responsibility.

Configured roots must be existing physical directories, including in explicit mode.
Named overlays may introduce virtual descendants beneath those admitted roots.
SourceOverrides and separate ManifestOverrides require canonical root-relative
identities. Dirty bytes win; source tombstones remove disk members; selected-manifest
tombstones invalidate pinned project mode. Adding/deleting a nested manifest overlay
changes boundary membership without silently selecting another project root.
Captured overlay labels are normalized to their admitted physical source paths,
preventing arbitrary caller labels from colliding in diagnostic ownership.

Shared SourceGraph resolution keeps portable components, exact case/case-sibling
checks, containment, symlink/alias rejection, imports and overlay-over-host checks.
Project admission additionally rejects Windows reparse points and requires manifests
to be regular, alias-free, single-link files. Source hard-link policy remains the
existing VFS identity-collision rule, extended across parsed D and loaded E. One
physical source cannot acquire two VFS identities, including through dirty overlays;
a single source identity is not rejected solely because another native link exists.
The legacy adapter keeps its original policy.

An exact nested manifest is a boundary even if its TOML is malformed. Parent
inventory stops at that boundary; parent imports and explicit entries cannot cross
it. Wrong-case/colliding boundary names diagnose. Exclusions apply before descending
into skipped subtrees. `.git`, `.void`, `build`, `libs`, stdlib and external-package
domains are not automatic seeds. In-root imports from excluded paths retain ordinary
loader checks and semantic rules.

D limits are 4,096 eligible sources and 64 MiB aggregate captured eligible bytes.
Named overlays count in those limits; tombstones remove members. Inspected directory
work is capped at 16,384 entries; discovery paths at 64 descendant levels. Source
roots/exclusions retain Slice 1 limits and validation. E-only source loading retains
the existing import/parser limits instead of consuming D's byte/file allowance.
Bounds fail the whole selection, never a truncated search declared unique.

Before accepting success, recapture the relevant membership, source bytes, physical
spelling/admission, manifest and boundary observations against the same frozen
overlays. Changes yield VOID5011, “Project changed during discovery; retry.” This is
detection/retry, not an OS transaction or security sandbox against concurrent
filesystem writers. Returned metadata, source snapshots and diagnostics remain
independently owned after caller inputs and temporary parser trees are destroyed.

The SHA-256 selection recipe binds a format tag, canonical root, exact manifest
bytes, entry/mode, captured membership/bytes and observed directories/boundaries.
It extends the application compatibility contract and CLI cache identity. Project
analysis and applicable inventory recapture precede cache lookup. Linux cold/warm
tests require miss/hit, then ambiguity when a second main appears, and new misses
after name/profile/project-ID edits. No cache hit authorizes stale uniqueness.

## Compatibility and acceptance matrix

| Cases | Implemented and exercised behavior |
| --- | --- |
| P01–P03 | app/program/game/deep launcher names; explicit and directory/cwd project selection; check, normal/O2 JIT and independently linked normal/O2 native executables return 23 without numeric printing. |
| P04/P08 | Competing main.void and another seed diagnose in sorted order; importing another seed does not remove its candidacy. |
| P05 | No automatic app main fails check/run/build; explicit standalone check still accepts declarations-only sources. |
| P06 | Empty/nonempty libraries check without entry; no implicit root constants from another library source; imports restore ordinary access; project run/build/emit reject. |
| P07/P25 | libs/ and explicitly excluded imported helper mains remain helpers; imported global effects run once in E. |
| P09 | Parameter/default-parameter, generic, async and wrong-result mains reject; method/nested main names alone do not qualify. |
| P10–P12 | D syntax errors reject; D-only type errors/globals remain unvalidated/uninitialized; E errors and imported executable statements retain existing diagnostics. |
| P13/P14/P17 | Manifest/CLI overrides, scripts, legacy precedence and forwarded numeric input; explicit override ignores unrelated D ambiguity/syntax errors; invalid override never falls back. |
| P15/P16/P18 | Bare files beside invalid manifests, manifestless main.void, legacy numeric output/status, literal application arguments, rooted imports and selected-entry root constants/globals. |
| P19 | Malformed nested TOML still excludes child seeds; parent imports/overrides across it reject. |
| P24 | Exact file/byte/visited-entry boundaries, over-depth discovery, wrong case, source hard-link collisions/overlays, manifest link count, symlinks, unreadable files and physical-root requirements. |
| P26 | New/dirty/tombstoned named sources, virtual parents, source signature changes and TOML boundary/selected-manifest tombstones through the compiler API. |
| P27 | Continuous source-byte mutation and a Linux read-notification-triggered new child boundary cause rejection/retry; returned old snapshots remain unchanged. |
| P28 | Revalidate before cache, reject a new second main, change keys on manifest/name/profile/project-ID changes. |

Exact main-name ambiguity ranges are asserted. Invalid TOML/field/range, SemVer,
returned-metadata ownership and no-authority parser regressions from Slice 1 remain
enabled. Existing module, numeric, generic, tooling/session, LSP, globals, low-stack,
workflow, cache, CLI/resource and JIT/AOT regressions are included locally.

Dirty overlays are exercised at the compiler API. Project-aware ToolingSession,
voidls revision/stale-publication protocol, editor root selection and UI are **S3
or later**, not claimed from these API tests. CLI `--profile` and applying parsed
profile fields remain later integration; S2 keeps O2, `--no-opt` and cache flags,
and binds authored profile bytes into selection identity. Runtime app metadata,
zero-ID OS calls, creation, numeric enums, C FFI, resolver and raylib are untouched.

Project live-session/reload behavior is unqualified. The current selection digest
also makes the new project program's compatibility contract conservative, including
source bytes. Before enabling project reload, S3/S4 must reconcile that tag with
the roadmap's separate build identity and semantic context so compatible body-only
changes do not become metadata restarts. Existing legacy reload contracts are
unchanged; this tag is not acceptance of a new project reload design.

## Executed qualification

| Final frozen-source qualification | Release | Debug | Fatal strict |
| --- | --- | --- | --- |
| Linux broad selected CTests | 35/35 PASS | 35/35 PASS | 35/35 PASS |
| Deliberate child-project CTest | 1/1 PASS | 1/1 PASS | 1/1 PASS |
| Compiler API selection/ownership/admission fixture | 339 checks PASS | 339 PASS | 339 PASS |
| Same complete API fixture, hard 1 MiB main stack | 339 PASS | 339 PASS | 339 PASS |
| Real project CLI/JIT/AOT fixture | 88 processes PASS | 88 PASS | 88 PASS |
| Deliberate child selection/boundary execution fixture | 11 processes PASS | 11 PASS | 11 PASS |
| Updated bare-source/manifestless compatibility fixture | 17 checks PASS | 17 PASS | 17 PASS |

This is **108/108 selected configuration/test outcomes**, not the entire repository
suite. Assertion/process counts are not distinct source-language test-case counts;
they repeat across profiles. The nested fixture was added as one focused CTest per
profile after the broad 35-test runs, with no product-code changes. It proves the
parent returns 17, deliberate child selection returns 23 through normal/O2 JIT and
independent AOT, and malformed child configuration never causes ancestor fallback.

Release `cli_install` additionally passes 1/1. Separate real compiler/tooling
component installs preserve all five exact TOML source/license/provenance files and
pass installed offline reconstruction. Six installed-project operations pass check,
normal/O2 JIT, AOT build/execution and native execution after the source tree is
hidden and standard-source override points at an absent directory. Ten install,
provenance and execution commands are retained in that supplementary receipt.

Windows 10 native Release/Debug each pass the 309-check API fixture, 88-process
project execution fixture and 17-check legacy fixture. Each profile also passes the 11-process deliberate-child native
supplement, recorded separately in the receipt. Native compile/link commands,
new and retained object/archive hashes, provider verification and source inventory
are retained. The original Windows CRLF-oracle failure remains alongside the
corrective native results.

Linux uses the existing PikaOS 4 x86_64 LLVM/Clang/LLD 23.1.4 build trees. Strict
remains RelWithDebInfo with ASan/UBSan and exactly:

```text
ASAN_OPTIONS=detect_leaks=1:halt_on_error=1:exitcode=87
UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1:exitcode=86
```

The S1 syscall-denial fixture still proves four live SIGSYS controls, then pure
parsing without file/process/network/mutation authority. Its existing filtered-
child `_Exit` remains; ordinary strict tests separately retain fatal exit-time
leak checking. Discovery itself performs authorized filesystem reads; it executes
no source initializer, installer, resolver or native load during analysis.

Windows is **Windows 10 x64 build 19045**, regardless of the VM display label.
Qualification uses an incremental native relink: current compiler.cpp, project.cpp,
main.cpp and selection tests are freshly compiled for Release/Debug against the
existing verified LLVM 23.1.4 and unchanged core/backend/Runtime/provider objects.
The baseline source inventory verifies unchanged implementations; changed public
headers add APIs/enum values without changing existing exported object layouts.
Fresh compiler identities include the rebuilt object hashes and actual retained
dependency inputs. Existing `/MT`, iterator/RTTI ABI, Debug symbols, Release O2,
LongPathAware manifest, native resources and provider verifiers are retained.
Original native executables/source/receipts are preserved under different filenames.

This is actual native API/CLI/JIT/AOT execution, **not a clean current-tree Windows
CMake rebuild**, Windows Server 2022 acceptance, macOS qualification or hosted CI.
Windows symlink privilege is unavailable; those native symlink/reparse fixtures
are UNQUALIFIED. The existing Windows persistent-cache backend reports unavailable;
selection-before-cache rejection is tested without claiming a Windows warm hit.
Linux supplies the cache, symlink and read-notification boundary evidence. Windows
fatal strict, actual project editor UI and the final seven-job gate are unexecuted.

## Failures retained and corrected

Original Slice 1 `[=roject]` assertion/UBSan failures and the corrected private TOML
guard/pin are untouched. S2 retains initial failures, exact inputs/recipes and
corrective results in its receipt/evidence rather than overwriting the old record.

Actual implementation regressions found during S2:

- Named overlays could make a nonexistent configured root appear admitted. The
  physical-root gate now rejects this before selection; virtual descendants still work.
- Lexical `..` normalization before canonicalization could select the wrong physical
  sibling across an allowed ancestor alias. Canonicalization now preserves OS path
  meaning, while selected-root/source aliases retain their admission rules.
- Scratch source-link admission was stricter than the approved existing VFS policy.
  The retained example shows bare check accepting one source identity while the
  scratch project rejected it. Final source checks preserve that single-identity
  behavior and reject D/E collisions; manifests still require one link.
- Scratch aggregate capture charged E-only imports against D's exact 64 MiB budget.
  Final accounting applies the new budget to eligible discovery bytes and keeps
  ordinary E loader limits; the inclusive-boundary regression imports an E-only helper.
- The parse/load split initially read source 129 before the 128-level import guard,
  exposing its invalid UTF-8 instead of the existing nesting diagnostic. Admission
  now precedes parsing; legacy and explicit-project regressions require VOID1107
  without reading/parsing that over-depth source.

Test/setup corrections also remain recorded: missing direct LLVM StringRef include;
an incorrect build-target name; diagnostic expectations that disagreed with the
existing parser; an os.args fixture that omitted its existing result match; a
leftover eligible helper main after exclusion reset; and native legacy CLI CRLF
expectations. The Windows oracle preserves exact existing numeric stdout instead
of changing production output or normalizing all runtime output. No test, sanitizer,
ACL, provider check or accepted negative case was disabled.

Earlier broad green runs are retained with their original source scope. The final
matrix and native receipt identify the frozen corrected inputs; earlier results
are not substituted for qualification of the final code.

## Inventory and next review boundary

Final expanded Git status contains **25 modified and 138 untracked files (163 paths)**. Of the 83 reviewed starting paths, 72 are unchanged and 11 carry identified S2 amendments. Seventeen source/documentation paths form the S2 delta; the combined source scope is 41 paths. New reports, receipts and raw evidence are separately listed. HEAD remains the baseline and the index is empty.

All reviewed S1 reports, receipts, raw failures, TOML source/license/pin/patch,
protected Runtime/stdlib/editor/native-admission inputs and CI workflows remain
unchanged. S1 source/document paths amended by S2 are explicitly identified in
the inventory; untouched S1 changes are not presented as new S2 implementation.
Build/native artifacts are local evidence, not published binaries.

Stop here for Golden Wrench's independent S2 review before S3. Next scope is the
existing roadmap's CLI/profile and ToolingSession/voidls project protocol/session
integration, with revision/stale-result guards and existing editor ownership.
Resolve material discrepancies with Boss/Golden Wrench before dependent changes.
The full future roadmap remains, including dynamic-default general C FFI/raylib,
optional separately qualified static modes and the mandatory real application
demonstration. Hosted CI stays on strict hold until that complete milestone, then
one integrated seven-job acceptance gate remains under Boss's authority.
