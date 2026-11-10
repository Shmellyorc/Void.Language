# First-class VOID projects: Slice 1 engineering record

8 November 2026. Starting HEAD `2f667ea338127b8f91c3136804377ef3fa5218d6`.
Boss authorized bounded engineering after contract/source reconciliation. The
fifteen preexisting Phase 1 documentation changes remain in the working tree.
**Implemented and locally qualified on Linux; independent review pending. Native
Windows/macOS and hosted acceptance are UNQUALIFIED.** No Git integration or
publication is performed. [Machine-readable receipt](evidence/slice1-local.json).

## Delivered foundation and contract confirmation

`void_core` now exposes [project.hpp](../../include/void/project.hpp), implemented
in [project.cpp](../../src/frontend/project.cpp). It recognizes schema-1 application
and library metadata, exactly the fields/defaults in the
[central schema contract](../PROJECT_ARCHITECTURE_ROADMAP.md#proposed-manifest-and-identity-model).
No entry field or entry filename is required. This is a compiler-library foundation,
not active configuration for existing voidc/voidls commands.

The source audit confirmed no material conflict blocking this slice. Current
analysis derives its root from an entry; Runtime arguments are admitted before
globals; os getters require explicit IDs; adapters use the existing entry-only
preflight. None is repurposed to consume manifest data. Two distinctions settle
otherwise ambiguous validation requirements:

- This parser validates supplied bytes/fields, not a physical project. Filename,
  exact physical case, roots/entry existence, symlinks/reparse/hardlink identity,
  nested boundaries and filesystem containment admission remain S2 responsibilities.
  Source roots/entry paths receive lexical validation here, with no filesystem probe.
- Project/application/package IDs are independent roles. Equal explicitly authored
  text across roles is valid. Duplicate key/table/path declarations reject; collision
  between distinct physical projects in one owning namespace is later admission,
  not an implicit process-global identity registry in the parser.

Example explicit API use; callers retain ordinary diagnostic ownership:

```cpp
auto result = vd::project::parse_manifest(vd::SourceFile("void.project.toml", bytes));
if (!result.succeeded()) throw vd::CompilationError(result.diagnostics);
auto metadata = result.manifest; // shared_ptr<const Manifest>, owned strings/spans
```

`SourceFile` performs existing strict UTF-8 validation first. The parser checks
its captured byte size before TOML parsing. A future file reader must cap ingestion
before constructing a snapshot; parsing cannot retroactively bound a caller's
allocation. A source label alone never proves path admission or trust.

## Schema, versions and diagnostics

Required project ID/name/version/kind, kind-specific application/package sections,
optional entry/name/version overrides, source arrays and debug/release profile
metadata are validated. Defaults are copied into the immutable result. Strings,
integers, arrays and tables receive exact type checks; no coercion. Unknown,
missing, duplicate, forbidden and malformed fields yield existing `Diagnostic`
records. Nonempty `dependencies`/`native` declarations reject as reserved; an empty
table is inert. Parsed profile fields are not applied to current builds.

All three identity roles use the existing OS ID specification. Package import
names reuse the compiler identifier predicate. Display names use strict UTF-8,
1–128 scalars, no C0/C1 controls and at least one non-space scalar. Manifest paths
share the extracted lexical `portable_path_component` helper with the existing
compiler; source normalization, root admission and error messages remain unchanged.
Root/exclusion duplicates, case conflicts, overlapping roots and reserved seed-root
components reject without opening paths.

`parse_version` retains 1–255-byte exact version identity and separately compares
SemVer precedence. Decimal core/prerelease components compare without machine-width
conversion; build metadata never changes precedence. Numeric leading-zero rules,
ASCII identifiers and release/prerelease ordering are enforced. No resolver,
constraint grammar or dependency installation is introduced.

TOML comes from the private MIT-licensed [toml++ pin](../../third_party/tomlplusplus/README.md).
It is vendored, so configuration/build/validation require no parser download.
The public API contains no toml++ types. The adapter preserves scalar-to-UTF-8 byte
ranges and earlier duplicate-key locations in owned SourceFile snapshots. Errors
sort deterministically by source offset/range/code/description/hint. A failed parse
returns no Manifest, even if some fields were valid.

Bounds are 256 KiB input, eight nested values, eight key segments, 32 source roots,
256 exclusions and 128 diagnostics plus one overflow notice. The TOML key limit is
applied before recursively shaped tables can be allocated. VOID5001–VOID5008 are
assigned in the existing [diagnostic registry](../DIAGNOSTICS.md#slice-1-project-manifest-diagnostics-8-november-2026);
VOID9001 reports resource limits, and SourceFile retains VOID1001 for invalid UTF-8.
These numeric codes are implemented; later boundary/entry diagnostic design is not.

## Executed local qualification

Host: PikaOS 4 x86_64, LLVM/Clang/LLD 23.1.4, existing verified native provider
prefixes. Existing `build/llvm23-release`, `llvm23-debug`, `llvm23-strict` trees were
configured and rebuilt. Strict is the existing RelWithDebInfo + ASan/UBSan profile.

| Final check | Release | Debug | Strict |
| --- | --- | --- | --- |
| Selected CTests: four new manifest suites plus twenty existing regressions | 24/24 PASS | 24/24 PASS | 24/24 PASS |
| In-memory schema/version/diagnostic/mutation fixture | 3,442 checks PASS | 3,442 PASS | 3,442 PASS |
| Same fixture under syscall denial, plus four live SIGSYS controls | 3,443 checks PASS; four controls PASS | same | same |
| Explicit-file/directory/CLI/AOT compatibility fixture | 15 checks PASS | 15 PASS | 15 PASS |
| Full manifest fixture with a hard 1 MiB main stack | 3,442 checks PASS | 3,442 PASS | 3,442 PASS |

The counts within the manifest fixture include assertions and deterministic
reparses, not 3,442 distinct test cases. The filtered count adds its filter-install
assertion. These are repeated profiles, not one fictional aggregate case count.
The selected CTest matrix is **72/72 configuration/test outcomes**, not the entire
repository suite. Final Release `cli_install` separately passed 1/1. Independent
compiler and tooling component installs retained the exact new MIT license,
provenance and patch files; both passed hash/content checks.

Existing regressions cover CLI contracts/resources, frontend, modules, numeric
semantics, validated analysis, tooling/sessions, real source/module/numeric JIT/AOT,
local libraries, Stage 9 workflow, disk caches, OS error/frontend behavior and LSP.
Their actual names, results and timings are in the receipt. Current discovery,
explicit source execution, default outputs, absent zero-ID OS APIs and unsupported
project/init/new options are exercised by `tests/project/legacy.py`, with an invalid
manifest beside the source. The source and manifest remain unchanged.

Strict commands retain exactly:

```text
ASAN_OPTIONS=detect_leaks=1:halt_on_error=1:exitcode=87
UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1:exitcode=86
```

The Linux denial wrapper first proves file-open, process-exec, network-socket and
namespace-mutation controls die with SIGSYS, then runs the entire parser fixture
under that filter. It blocks native loader file opening too. Shell/environment-
looking values remain literal strings; reserved install/load/hook declarations
reject. No network/native installation or execution occurs during validation.
Filtered children use `_Exit` after reporting because LeakSanitizer's own exit-time
procfs/helper operations would violate the filter. Ordinary strict runs separately
retain fatal ASan/UBSan and exit-time leak checking; no sanitizer recovery is enabled.

## Retained failures and repair

Early local build iterations corrected an initial unconfigured target request,
a misplaced private vendor field, use of vector resize with non-default-constructible
Diagnostic, and a test string's deduced pointer type. A negative fixture incorrectly
classified `a.CON/file.void`: the established reserved-device rule applies before
the first dot, so this is valid; the fixture now tests a real `CON.txt` component
and retains the valid spelling as positive coverage. None changes the VFS policy.

The first full qualification exposed a genuine parser defect under mutation:
[the exact rejected snapshot](evidence/slice1-malformed-header.toml) contains
`[=roject]`. Header parsing entered parse_key with an invalid key starter. Upstream's
precondition assertion aborted Debug; fatal UBSan rejected its optimizer assumption
in strict. Both parser CTests failed in each profile (22 passed, 2 failed); the
first Release run passed but did not establish safe handling of this input.
The strict filtered child also terminated with SIGSYS while reporting the defect;
its exact blocked reporting syscall was not separately traced.

The private patch now performs an ordinary runtime key-start check before the
assumption and produces a source diagnostic. It does not disable assertions,
sanitisers, inputs or tests. The explicit malformed-header regression and mutation
corpus pass in all final profiles, including denial and 1 MiB stack tests. Debug's
GDB trace, original failed CTest XML/log hashes and the exact input hash are retained
in the receipt/local build evidence. A preexisting tests/tests.cpp missing-field
initializer warning remains; no new manifest-code warning or suppression is added.

## Remaining scope and handoff

There is no manifest loader/discovery path, physical admission, entry-source scan,
embedded metadata, Runtime app context, zero-ID OS call, init/new, editor prompt,
new LSP protocol, enum/as?, FFI/raylib or dependency resolver in this slice. Libraries
receive parsed metadata only; source-only library analysis is also later work.
Existing application/native/VOIDUP behavior is not opted into this API.

Native Windows/macOS execution, actual editor UI and the hosted seven-job matrix
were not run or accepted here. The existing workflow, fatal sanitizer settings,
provider pins, receipts and historical CI evidence remain unchanged. Boss controls
Git integration, hosted CI and publication. Stop here for Golden Wrench's independent
review; this record does not authorize S2.

## Exact working-tree inventory

All paths below are relative to the repository, against the stated HEAD. `M` means
modified and `??` means new/untracked; **nothing is staged**. The fifteen rows marked
Phase 1 were already modified before this engineering task; their architecture work
is preserved, with current Slice 1 status added where relevant. The rest is the
bounded implementation, tests, vendored source/notices and evidence. Build logs and
binaries remain ignored local artifacts and are not publication.

| Status | Exact path | Preexisting Phase 1 change |
| --- | --- | --- |
| M | [CMakeLists.txt](../../CMakeLists.txt) | no |
| M | [cmake/InstallLicenses.cmake](../../cmake/InstallLicenses.cmake) | no |
| M | [docs/CLI.md](../CLI.md) | yes |
| M | [docs/DECISIONS.md](../DECISIONS.md) | yes |
| M | [docs/DIAGNOSTICS.md](../DIAGNOSTICS.md) | no |
| M | [docs/EDITOR_INTEGRATION.md](../EDITOR_INTEGRATION.md) | yes |
| M | [docs/LANGUAGE_BIBLE.md](../LANGUAGE_BIBLE.md) | yes |
| M | [docs/LANGUAGE_SERVER.md](../LANGUAGE_SERVER.md) | yes |
| M | [docs/LOCAL_LIBRARIES.md](../LOCAL_LIBRARIES.md) | yes |
| M | [docs/MODULE_SYSTEM.md](../MODULE_SYSTEM.md) | yes |
| M | [docs/NATIVE_LIBRARIES.md](../NATIVE_LIBRARIES.md) | yes |
| M | [docs/PROJECT_ARCHITECTURE_ROADMAP.md](../PROJECT_ARCHITECTURE_ROADMAP.md) | yes |
| M | [docs/README.md](../README.md) | yes |
| M | [docs/RUNTIME_GLOBALS.md](../RUNTIME_GLOBALS.md) | yes |
| M | [docs/SOURCE_COMPILER.md](../SOURCE_COMPILER.md) | yes |
| M | [docs/STAGE8_DISCOVERY.md](../STAGE8_DISCOVERY.md) | yes |
| M | [docs/STAGE9_APPLICATION_WORKFLOW.md](../STAGE9_APPLICATION_WORKFLOW.md) | yes |
| M | [docs/os/API.md](../os/API.md) | yes |
| ?? | [docs/projects/SLICE1.md](SLICE1.md) | no |
| ?? | [docs/projects/evidence/slice1-local.json](evidence/slice1-local.json) | no |
| ?? | [docs/projects/evidence/slice1-malformed-header.toml](evidence/slice1-malformed-header.toml) | no |
| M | [include/void/diagnostic.hpp](../../include/void/diagnostic.hpp) | no |
| M | [include/void/path.hpp](../../include/void/path.hpp) | no |
| ?? | [include/void/project.hpp](../../include/void/project.hpp) | no |
| M | [src/frontend/compiler.cpp](../../src/frontend/compiler.cpp) | no |
| ?? | [src/frontend/project.cpp](../../src/frontend/project.cpp) | no |
| ?? | [tests/project/authority.py](../../tests/project/authority.py) | no |
| ?? | [tests/project/legacy.py](../../tests/project/legacy.py) | no |
| ?? | [tests/project/manifest.cpp](../../tests/project/manifest.cpp) | no |
| ?? | [tests/project/provenance.py](../../tests/project/provenance.py) | no |
| ?? | [third_party/tomlplusplus/LICENSE](../../third_party/tomlplusplus/LICENSE) | no |
| ?? | [third_party/tomlplusplus/README.md](../../third_party/tomlplusplus/README.md) | no |
| ?? | [third_party/tomlplusplus/toml.hpp](../../third_party/tomlplusplus/toml.hpp) | no |
| ?? | [third_party/tomlplusplus/upstream.json](../../third_party/tomlplusplus/upstream.json) | no |
| ?? | [third_party/tomlplusplus/void-bounds-and-locations.patch](../../third_party/tomlplusplus/void-bounds-and-locations.patch) | no |
