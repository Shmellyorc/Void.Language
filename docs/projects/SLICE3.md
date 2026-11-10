# Slice 3: project sessions, voidls protocol and CLI profiles

8 November 2026. Baseline and unchanged HEAD:
`2f667ea338127b8f91c3136804377ef3fa5218d6`. Golden Wrench accepted the preserved
[Slice 2 final closeout](SLICE2_FINAL_CLOSEOUT.md) for this local continuation.
**S3 is locally qualified. Stop for Golden Wrench's independent review before S4.
Hosted CI remains on strict hold until the full project/raylib milestone.**

## Implementation and ownership

The existing [ToolingSession](../../include/void/tooling.hpp) now accepts a pinned
manifest root and optional relative entry override. The entry-path constructor
remains legacy. Owned revision tokens distinguish sessions and revisions; retained
source and TOML overlays have separate namespaces. Selection, analysis and
acceptance remain separate. A failed selection retains its diagnostic source bytes
and available D/E observations, with neither an executable analysis nor a current
semantic index. Valid parsed metadata is retained when available; malformed TOML
keeps its raw bytes in the captured diagnostic/overlay and supplies no source policy.
Library checks retain a declarationless semantic index for navigation, with no
application entry or executable program; library metadata lowering rejects.

The compiler owns the shared ancestor walk, manifest and source admission, D/E
selection, parsing, semantic indexes and recovery. The project document admission
check uses the same SourceGraph and checks open source overrides against the pinned
boundary. D-only application sources remain parse-only; E and the checked library
union supply semantic navigation. There is no second parser, editor entry resolver,
LLVM lowering path or project native authority. The existing recapture checks and
error classification remain; this is not an atomic filesystem snapshot.

The full conservative source/manifest/boundary digest stays selection and build
provenance. Manifest project/application/package IDs stay validated metadata;
they are not derived from source hashes. The compatibility contract uses framed
normalized project context plus the existing graph, declaration and global contract.
Effective CLI profile settings are separately recorded in the cache identity. No
Runtime application metadata is embedded. A body-only edit changes provenance and
can retain native compatibility; a semantic metadata, root, entry or source-policy
change requires reselection/restart. After successful reselection, TOML and D-only
observations are not unknown E-module hints. Actual normal/O2 JIT replacement
retains global cells (observed 11 → 22) and rejects changed context or stale tokens.
Existing legacy reload behavior remains under the same backend gate.

## Protocol and editor compatibility

| Mode | Delivered behavior |
|---|---|
| Legacy `--discover` / `--entry` | Original arguments and three-field discovery JSON; tooling protocol 1. Absolute and relative legacy entries retain entry-parent scope. |
| `--discover-project WORKSPACE ACTIVE ROOT ENTRY` | Additive schema-2 summary, 64 KiB bound, nearest admitted manifest in workspace unless root/legacy entry pins selection. Explicit root never follows an outside/child active source. |
| `--project DIR [--entry PATH]` | Pinned project, including no-main, malformed TOML and library sessions; `voidProjectProtocol: 1` in addition to `voidToolingProtocol: 1`. |
| Project wire updates | Source/TOML full-text overlays, file creation/rename/delete, watch invalidation (including wrong-case manifests), owned UTF-16 diagnostics, project revision summaries, same-root explicit reselection. |
| VS Code | Compiler preflight, literal `void.project.root`, retained configured/bundled/PATH and standalone behavior, trust and one-filesystem-folder restrictions. Client/revision guards reject stale query/edit/state results; root switching starts a fresh session. |
| Older server | Automatic/manifest preflight reports incompatibility; no downgrade. Explicit legacy entry can still use the old endpoint. |

`void/projectState` supplies the current summary and notifies on accepted revisions.
`void/reselectProject` is deliberate same-root selection. A dirty rename carries
owned bytes even if its deletion watch arrives first; a deletion does not fall
back to disk. UTF-16 assertions include an astral character earlier on the same
TOML line. No first-file offers, init/new, project creation or automatic manifest
writing were implemented. [Protocol details](../LANGUAGE_SERVER.md) describe
revision representation and limits.

## Profiles and qualified debug information

`--profile debug|release` is project-only on check/run/build/emit. Release is the
default, then manifest settings, then explicit `--no-opt`. An implicit release may
yield to `--cache` selecting debug, but explicit release or effective debug/O2
conflicts with `--cache`. `--no-cache` remains run-only and controls persistence.
Literal forwarding, standalone flags and application exit status remain intact.

Boss approved visible, source-spanned **VOID5012** only for the selected unsupported
`debug_info="line"` or `"full"`. `none` is quiet, and all requested values stay in
manifest metadata. The backend already emits source/type/location debug metadata;
what is deferred is selectable profile-controlled emission. No requested mode is
claimed as emitted, and `none` does not newly strip baseline records. The emitter
itself was not changed. Native C++ `/Zi` in the Windows Debug toolchain is unrelated
to VOID's requested project modes. Defaults span manifest EOF; authored settings
span their exact TOML value. [Decision record](evidence/slice3-profile-decision.txt).

The actual object instruction counts for the fixture's `compute` function are
O0/O2/release with explicit `--no-opt`: **6/2/6 on Linux, 8/2/8 on Windows 10**.
Authored reverse optimization overrides also change the emitted instruction stream.
Independent AOT and both JIT modes execute, with real Linux cache cold/warm hits
and conflicts. Windows disk-cache persistence stays unavailable; profile execution
and selection-before-cache rejection still run. No flags are merely accepted and
ignored for optimization.

## Executed qualification

| Configuration | Core/CLI regression matrix | Final server regression matrix |
|---|---:|---:|
| Linux Release | 42/42 | 33/33 |
| Linux Debug | 42/42 | 33/33 |
| Linux fatal ASan/UBSan (RelWithDebInfo) | 42/42 | 30/30 |

The matrices total 222 selected CTest outcomes, not the entire repository suite.
The last server-only relative-entry repair was tested on freshly rebuilt servers
across the complete registered LSP selection; unrelated CLI/core binaries did not
change. Three existing small-stack-only LSP tests are not registered in the strict
configuration. Strict retains `VOID_SANITIZERS=ON`,
`ASAN_OPTIONS=detect_leaks=1:halt_on_error=1:exitcode=87` and
`UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1:exitcode=86`. New focused counts
per Linux profile: 157 session/profile/actual reload assertions, 43 profile process
checks, and 74 final project protocol checks. The old 476 selection assertions,
parser/header/provenance/no-authority checks, actual project and standalone JIT/AOT,
imports, cache, native admission, Runtime globals/OS, numeric/generic, LSP and
compatibility tests remain in scope.

**Windows 10 x64 build 19045, native Release and Debug:** each passed 8,788 parser,
353 selection, 157 session/reload, 43 profile, 74 final project protocol, 88 project
CLI, 11 nested and 17 legacy checks; header, offline provenance and four legacy LSP
process suites passed. Changed frontend/source/tooling/manifest/main and LSP units
were rebuilt, as was the LLVM backend that consumes the updated session headers.
The verified Runtime/providers and original resource checks were retained. A final
LSP-only rebuild relinked against the hash-checked same core objects and repeated
both project and legacy wire tests. This is incremental native qualification,
not a clean current-tree CMake build. HTTP/host dependency verifiers passed in
both profiles. No pins, ACLs, native/provider or package checks were relaxed.

**Actual editor and installed components:** Linux VS Code 1.141.0 ran 14 project
host checks, including symbols, dirty TOML, recovery completion, rename, deletion, ambiguity,
project switch and old-session result rejection. The inherited trusted, restricted
trust and multi-root editor host suites pass. Seven Node adapter tests include an
actual older voidls binary and retained explicit legacy mode. Separate real compiler
and tooling installs preserve exact TOML corresponding-source/license files and
offline reconstruction. Installed project profiles and JIT/AOT pass, including
native execution with application source hidden and an invalid stdlib override.
Final installed tooling repeats the complete 74-check project wire fixture; existing
`cli_install` also passed 1/1. No VSIX/binary publication or hosted run was performed.

| Roadmap gate | Evidence in this slice |
|---|---|
| P26 | Dirty source/TOML, same-line UTF-16, virtual creation, rename/delete, nested boundary overlays, disk parity, library/no-main error recovery. |
| P27 | Existing recapture tests, delayed/duplicate/cross-session acceptance rejection, queued LSP updates, real editor switch and no stale semantic views. |
| P28 | Actual Linux cache hits and selection revalidation on second main, metadata/profile changes; project/legacy provenance stays separate. |
| P40 | Real restricted/multi-root host no-spawn controls, outside/nested root protection and old-server fail-closed preflight. |
| P41 | Actual optimization and AOT/JIT, profile/cache conflicts, standalone compatibility, literal forwarded arguments and propagated app statuses. |

## Original failures retained and corrected

1. Missing npm on PATH: used the existing pinned Node 24.19.0 installation.
2. Nullable-entry TypeScript errors in old tests: kept their exact path assertions
   and narrowed legacy results. No behavioral assertion was removed.
3. First adapter test used an old default server: selected the built matching
   executable and retained a separate deliberate old-server rejection control.
4. TOML revision IDs were treated as unresolved E modules: compared only semantic
   module hints after a fully validated project selection.
5. First cache pair observed the cache directory's own creation: bootstrapped that
   excluded infrastructure before the same cold/warm assertion; no cache-policy change.
6. Real editor deletion failed because Trash is unavailable: selected permanent
   deletion only in the isolated fixture profile.
7. Editor project-switch timeout: preserved pending restart intent across coalesced
   readiness requests; the real project-host corrective run passed.
8. Native input guard omitted the intentional LSP source change: reconciled the
   exact nine-file baseline source difference and kept the failed guard receipt.
9. Windows protocol oracle compared generic `/` paths with native `\` spelling:
   compared exact generic path spelling; unchanged native Release binary passed.
10. Native legacy protocol invocation omitted its fixture directory: supplied the
    same required argument as CTest, preserving the original exit and stderr.
11. Relative `voidls --entry` startup used an empty parent before absolutizing:
    restored the legacy absolute-entry-parent calculation. Added real relative-entry
    initialization/navigation/shutdown and reran final servers on all profiles.
12. Receipt audit initially required an identical installed compiler file hash.
    CMake relocates ELF RUNPATH; verified identical build identity and bytes
    outside the dynamic metadata sections, and retained both whole-file hashes.
13. The raw index checksum audit differed. All 68,996 index paths, modes and blob
    IDs match HEAD exactly, and the staged diff is empty. Both raw checksums are
    retained; the index was not rewritten to force a match.
14. A legacy editor run overlapped another real host and failed to apply a typed
    edit. Ran the foreground typing suite alone and retained the overlapping run.

Original logs, fixtures, native receipts and corrective results are in the
[machine-readable receipt](evidence/slice3-local.json) and its raw artifacts.
No test, sanitizer, security check or dependency pin was disabled.

## Exact changes and review boundary

- [S3 source delta](evidence/slice3-source.patch): 29 source/documentation paths
  against the accepted S2 working tree.
- [Combined source diff](evidence/slice3-baseline-source.patch): all 57 source and
  architecture paths against baseline HEAD.
- [Complete layered Git inventory](evidence/slice3-inventory.txt): every working-tree
  path, including retained S1/S2 artifacts and new S3 report/evidence files.

Both patches pass reverse checking and exact forward reconstruction in a temporary
location. Reports/evidence are inventoried separately to avoid recursive patches.
Of 247 starting paths, 234 remain byte-for-byte unchanged and 13 have identified
S3 amendments. All earlier S1/S2 reports, original failures and evidence are
unchanged. Index entries exactly match HEAD with no staged changes; raw index
metadata hashes are recorded separately. No staging, commits, pushes, CI dispatch,
dependency repins, ACL weakening or binary publication.

Remaining limits: Windows symlink/reparse fixture privilege and persistent cache
warm hits are unavailable; Linux supplies those negative/cache checks. Windows
fatal sanitizer, Windows Server 2022, macOS, Windows/macOS editor host, VSCodium,
installed VSIX and clean Windows CMake builds are unexecuted here. No Runtime
application identity/zero-argument OS adapters, creation, numeric enums, general
C FFI, resolver or raylib implementation is claimed. The complete dynamic-default
raylib and separately qualified static-mode goals remain in the roadmap. Stop at
Golden Wrench's S3 review; S4 has not started.

Final expanded Git inventory: 541 paths (36 modified,
505 untracked), including 278 new S3 report/evidence paths.
