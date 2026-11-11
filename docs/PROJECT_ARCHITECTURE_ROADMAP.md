# First-class VOID projects and native application roadmap

## Current Slice 5 implementation

Golden Wrench accepted S4 for local continuation. Slice 5 implements exclusive
`voidc init` and `voidc new` and the optional VS Code first-file creation workflow.
[Implementation and qualification](projects/SLICE5.md) records source changes and
the P36–P40 evidence. Independent S5 review precedes S6. The integrated project,
general C FFI and dynamically linked raylib milestone still precedes hosted CI.

The following slice records retain their original review scope.

## Slice 4 implementation

Golden Wrench accepted the [S3 final closeout](projects/SLICE3_FINAL_CLOSEOUT.md).
Slice 4 adds the versioned application record, per-Runtime admission before eager
globals, AOT embedding and four closed OS location adapters. [Implementation and
qualification](projects/SLICE4.md) records the ABI, tests and platform scope. Stop
for independent S4 review before S5. Hosted CI remains on strict hold until the
integrated project, C FFI and dynamically linked raylib milestone.

## Slice 3 implementation, 8 November 2026

Golden Wrench accepts the preserved [S2 final closeout](projects/SLICE2_FINAL_CLOSEOUT.md).
Boss authorizes project ToolingSession/voidls/editor integration and project-only CLI
profiles at the unchanged baseline. [S3 report/evidence](projects/SLICE3.md) records
the implementation and final local qualification; stop for independent S3 review
before S4. Historical S1/S2 status paragraphs below retain their dated scope.

S3 keeps compiler-owned selection and immutable D/E observations, error-bearing and
library metadata sessions, separate TOML overlays, owned revision acceptance, new
schema-2 preflight and `voidProjectProtocol: 1`, guarded VS Code project recognition,
and effective optimization/cache profiles. It separates raw selection/build provenance
from normalized semantic source context; source hashes do not become permanent Runtime
application identity. S3 deferred Runtime application metadata and OS convenience
adapters to S4.

Boss explicitly approved structured warning VOID5012 for the selected unsupported
line/full debug-information mode, retaining requested metadata, actual optimization
and existing backend behavior; none is quiet. The baseline already emits source/type/
location debug records, but does not implement selectable none/line/full modes. S3
does not claim those modes or strip baseline records. Actual mode-specific emission
remains a separate review milestone. Standalone behavior remains unchanged.

Hosted CI stays on strict hold until the complete project/general-C-FFI/dynamic-default
raylib milestone and single integrated seven-job gate. S4, creation UI/init/new, numeric
enums, general C FFI, dependency resolution and raylib remain unimplemented. All future
roadmap sections and optional, separately qualified static-linkage goals remain intact.


## Current Slice 2 implementation, 8 November 2026

Golden Wrench accepts Slice 1 closeout for local continuation. Boss authorizes
compiler-owned manifest selection, bounded physical admission/inventory, shared
parse-only D versus semantic/execution E selection, explicit entry overrides and
library source checking. [Slice 2 report and evidence](projects/SLICE2.md) record
the implementation against unchanged HEAD `2f667ea338127b8f91c3136804377ef3fa5218d6`.
The reviewed Slice 1 working tree is retained as the layer beneath this slice.

The API and CLI use the existing SourceGraph lexer/Parser, semantic validator,
typed IR gate and LLVM pipeline. Library checking has an explicit declarationless
root scope, without an executable entry. Source/TOML overlays are supported by the
compiler API; project-aware ToolingSession/voidls/editor protocol remains S3.
CLI profile application, Runtime metadata/OS adapters and creation remain later
slices. S1's historical implementation paragraph below describes its original
handoff; S2 status here supersedes only its discovery/selection deferral.

Stop for Golden Wrench review before S3. No hosted CI until the complete dynamic-
default raylib milestone and single integrated acceptance gate below. No pin,
native-provider authority, Runtime or accepted language/security contract changes.

## Continuing local authorization and final hosted gate, 8 November 2026

Boss authorizes continued local engineering through the approved roadmap, with
Golden Wrench's independent review at each architectural boundary. Slice 1 closes
at that boundary; [the closeout record](projects/SLICE1_CLOSEOUT.md) separates its
new evidence from the original [implementation record](projects/SLICE1.md).
Slices after the current S2 boundary remain unimplemented. Material unresolved schema, ABI, resolver or
language decisions require Boss/Golden Wrench resolution before dependent work.
Existing compiler, Runtime, VFS and editor ownership and language compatibility
remain the implementation constraints.

**No hosted CI is authorized before the complete project/raylib milestone.**
The final seven-job matrix below is one integrated acceptance gate, after all
approved features and a real VOID application are ready. It is not a per-slice
hosted gate. Local Release, Debug and fatal sanitizer tests, available native
Windows tests, negative security/ownership tests and exact source/evidence
inventories accompany each slice. Missing native execution remains UNQUALIFIED.
Boss retains staging, commits, pushes, hosted dispatch and publication authority.

Before requesting that final hosted gate, demonstrate the application calling
raylib through general C FFI with dynamic native linkage on supported platforms;
ABI-compatible structs, functions and constants; actual window creation,
rendering, input and shutdown; normal/O2 JIT and independent AOT where supported;
correct exercised callback/native-resource lifetimes; missing/wrong-library
rejection; manifest/application metadata integration; VS Code recognition and run
workflows; and installed application execution without the source checkout.
Static linkage remains optional and explicitly qualified by execution mode, or
identified as unsupported; it cannot substitute for dynamic linkage.

This authorization supersedes earlier HOLD/no-continuation language only within
the approved roadmap and review boundaries. It changes no dependency pin,
sanitizer, ACL, native admission, package verification or CI workflow. No binary
publication is authorized.

## Current Slice 1 implementation, 8 November 2026

Boss authorized bounded Slice 1 engineering at baseline
`2f667ea338127b8f91c3136804377ef3fa5218d6`, retaining the preexisting fifteen-file
Phase 1 documentation work. The initial schema field/default, identity, SemVer,
lexical path and diagnostic foundation is now implemented as the explicit compiler-
library API [project.hpp](../include/void/project.hpp) /
[project.cpp](../src/frontend/project.cpp). The [engineering record](projects/SLICE1.md)
and [local receipt](projects/evidence/slice1-local.json) separate execution evidence
from design approval and remaining platform qualification.

This API consumes an already captured UTF-8 SourceFile and does no file opening,
physical path admission, inventory/discovery, code generation or execution. Manifest
filename/case/symlink/hardlink/containment checks, root/entry existence and cross-
project ID collisions belong to later explicit admission; successful parsing does
not prove those facts. Equal text in different identity roles remains allowed;
duplicate declarations/path identities are rejected. No implicit registry is added.

Current voidc/voidls/editor, standalone/main.void discovery, Runtime/OS identity,
provider receipts, native linkage and outputs are unchanged. S2–S6, init/new, prompts,
enums, FFI/raylib and dependency resolution remain deferred and unimplemented.
Older HOLD/design-only statements below describe the original architecture
assignment; the continuing local authorization above now governs engineering.

8 November 2026. Documentation and architecture planning only, prepared by
Keyboard Clanker for Golden Wrench's independent review. Inspected baseline:
`7a3e02008db64ee185aa4665aac5c76e3aff4b2b`. No implementation, new acceptance,
Git integration, CI dispatch or publication is authorized by this record.

**Phase 1 contract refinement, 8 November 2026:** source-audited baseline
`2f667ea338127b8f91c3136804377ef3fa5218d6`, initially clean. The sections below
freeze a concrete **proposal for Boss/Golden Wrench approval**, not an approved
schema or implemented feature. Implementation remains **HOLD**. This refinement
supersedes the earlier roadmap's open schema/entry/source-set suggestions for the
initial project implementation; the enum/FFI/raylib/package roadmap remains future
work. Historical decisions and CI evidence retain their original scope.

Review navigation: [schema](#proposed-manifest-and-identity-model),
[metadata](#proposed-metadata-delivery-and-os-locations),
[discovery](#proposed-compiler-owned-entry-discovery),
[compatibility](#phase-1-compatibility-and-migration-matrix),
[ownership](#phase-1-ownership-and-data-flow),
[creation](#proposed-first-file-creation-and-terminal-workflows),
[acceptance](#phase-1-concrete-acceptance-matrix),
[slices](#recommended-implementation-order-and-acceptance-gates) and
[approval questions](#unresolved-design-questions-for-independent-review).

This coordinated plan belongs here because project selection, Runtime identity,
package resolution, native loading and editor sessions need one dependency order
and one set of authority boundaries. Existing API guides retain delivered behavior.
The [decision ledger](DECISIONS.md#8-november-2026--first-class-projects-and-general-c-interop)
records the owner direction; the [documentation index](README.md) links this plan.

## Status and approved direction

| Label | Meaning in this record |
| --- | --- |
| Approved direction | Boss and Golden Wrench's requested product/architecture decisions, recorded here; not implementation authorization. |
| Proposed design | Concrete recommendation for review, including all manifest keys and future command spellings. |
| Implemented behavior | Inspected baseline source or an existing implementation record, with its original evidence limits. |
| Unverified behavior | No applicable execution evidence; source integration alone establishes no working-behavior or platform acceptance claim. |

**Approved direction:** native `void.project.toml` projects; independent project,
application and package identities; semantic versions; dependency/native linkage
metadata; Debug/Release settings; optional entry override; installed application
metadata; project-aware OS location calls without repeating the application ID.
Metadata confers no filesystem, native-provider or installation authority.

**Approved direction:** offer first-file project creation in VS Code, allow decline,
avoid repeated prompts, retain standalone use, and provide `voidc init`/`voidc new`.
Never require a hardcoded entry filename or mandatory `entry` key. Discover one
valid `main()` deterministically within the selected application source graph;
diagnose ambiguity. Preserve explicit entries, standalone/top-level execution and
libraries without main. Use typed general C interoperability, **dynamic linking by
default and static linking optional**, with raylib as the first substantial target.
Plan checked numeric-backed enum conversion, IDE support and practical distribution.

**Product acceptance goal:** a developer installs VOID, creates a project, writes
an application in `app.void`, `program.void`, `game.void` or any valid source
filename, uses raylib, runs it and packages it through project commands without
assembling compiler/linker invocations by hand. This is a future usability gate.

## Baseline audit and reusable foundations

| Area | Implemented behavior and evidence | Design consequence |
| --- | --- | --- |
| Selection | [source.cpp](../src/frontend/source.cpp), `discover_project`: explicit regular `.void` file wins; otherwise exact-case `main.void` search within the selected workspace, nearest ancestor first for an active file; labelled standalone editor fallback. [Discovery contract](STAGE8_DISCOVERY.md). | Extend compiler-owned selection with manifest mode and a versioned result shared by CLI/adapters; preserve legacy mode. No editor scan for main. |
| Analysis and execution | [compiler.cpp](../src/frontend/compiler.cpp), `SourceGraph`/`ValidatedAnalysis`, [module system](MODULE_SYSTEM.md), [globals](RUNTIME_GLOBALS.md), [Stage 9](STAGE9_APPLICATION_WORKFLOW.md). Entry-reachable imports, snapshots, typed validation, one LLVM JIT/AOT path. | Extend graph construction before entry classification; preserve validation, initialization, ABI and diagnostics. |
| Local sources | [LOCAL_LIBRARIES.md](LOCAL_LIBRARIES.md), `use lib/raylib` maps to `/libs/raylib/raylib.void`; these are project-owned sources, not a delivered raylib binding or external package domain. | Keep current imports and visibility; separately model external package identities and trust. |
| Trusted providers | [native_binding.hpp](../include/void/native_binding.hpp), [native_binding.cpp](../src/ir/native_binding.cpp), [NATIVE_LIBRARIES.md](NATIVE_LIBRARIES.md). Explicit host descriptors, typed signatures/effects, fixture/sink adapters and closed stdlib bridges. | Reuse admission and independent IR checking; a source declaration must not admit itself. |
| Callbacks and leases | [callback.hpp](../include/void/callback.hpp), [callback.cpp](../src/runtime/callback.cpp), [native resources](../src/runtime/native_resource.cpp). Provider unregister/quiescence includes dispatches before Runtime admission; precise roots and exact generation leases. Forwarded callable generation transfer remains restricted. | Dynamic-library ownership must join this lifetime chain; no early unload or unproven general callable transfer. |
| Native artifacts | [native_build.cpp](../src/native_build.cpp), [CLI](CLI.md), [installed layout](install/README.md). Explicit Clang/LLD 23.1.4 resources, `void-native.txt`, `void-native-resources.txt`, SHA-256 resource checks, argv execution and staged publication. | Keep compiler resource inventories distinct from project metadata. Existing hashes establish consistency, not arbitrary publisher trust. |
| OS context | [os.void](../stdlib/os.void) requires IDs for four app location getters; [Runtime](../include/void/runtime.hpp) already owns application arguments. [Phase A](voidup/REPORT.md) adds `os.args()` beyond the original thirteen-query API. | Extend immutable per-Runtime application context, rather than global mutable identity or repeated caller IDs. |
| Enums and casts | [ADVANCED_LANGUAGE.md](ADVANCED_LANGUAGE.md), [NUMERIC_TYPES.md](NUMERIC_TYPES.md), compiler enum parser and `expression()`. Current enum variants use an internal tag/payload representation; parser explicitly rejects `as?`, and expression precedence has no `??` operator. | Internal tag ordinals are not public integer discriminants or a C ABI. Both requested operator spellings need grammar review. |
| Editor | [VS Code extension](../editors/vscode/src/extension.ts), [language service](LANGUAGE_SERVER.md), [tooling sessions](TOOLING_SESSIONS.md). Trusted workspace checks, compiler discovery preflight, selected-entry sessions, overlays, UTF-16 and guarded edits. | Add project selection/manifest metadata through this infrastructure; source analysis must never load a library. |
| VOIDUP and source packaging | [Phase B](voidup/PHASE_B_REPORT.md) implements a private VOID-native authenticated toolchain installer; [integration](voidup/INTEGRATION_REPORT.md) retains later native/editor/installed-source evidence and pending hosted scope. Packages include matching stdlib, Runtime/providers, resource inventories, notices and source materials. | Reuse verified artifact/capability primitives and receipts where appropriate. Toolchain installation is not project package resolution. Preserve each record's platform limits. |

The existing Stage 9/Stage 8 and VOIDUP CI results remain evidence for their
recorded revisions and scopes. This audit ran no compiler, editor or native tests.
Project manifests, new discovery, numeric-backed enum casts and general C/raylib
integration are **unimplemented and unverified at this baseline**.

### Phase 1 source audit at the verified baseline

Read-only source inspection confirms the following implementation gaps; none is
proof of new functionality or execution acceptance:

| Source/symbol inspected | Concrete baseline finding and required extension |
| --- | --- |
| [frontend.hpp](../include/void/frontend.hpp), `ProjectSelection`, `analyze_project` | Selection holds entry/root/standalone only; analysis takes an entry path, not a manifest/source-set snapshot. A nullable entry for library/error states needs a new project record. |
| [compiler.cpp](../src/frontend/compiler.cpp), `validated_project`, `SourceGraph::load`, `validate_graph`, `application_entry` | Root is canonical entry parent; load parses and recursively imports; validator assumes a root Unit. Entry classification gives legacy entry precedence. Separate capture/root inputs while retaining that classifier; library check needs an explicit declarationless root environment. |
| [application.cpp](../src/runtime/application.cpp), `Runtime::application_arguments`, [Runtime ABI](../src/runtime/abi.cpp), `runtime_entry` | Args are admitted once before globals; AOT creates Runtime in its existing bootstrap. Application identity metadata is absent. Reuse that admission ordering without claiming existing type metadata is app metadata. |
| [os.void](../stdlib/os.void), [os.inc](../src/frontend/os.inc), [call binder](STAGE8_CALLS_NUMERIC.md#ordinary-call-implementation) | Four public getters require one string; private OS operations are closed. Duplicate ordinary functions reject and defaults are pure constants. New zero-argument calls need explicit closed adapters, not a hidden general overload/default feature. |
| [VS Code config](../editors/vscode/src/config.ts), [extension](../editors/vscode/src/extension.ts), [voidls](../src/lsp/main.cpp), [tooling.hpp](../include/void/tooling.hpp) | Adapter calls legacy preflight, starts `--entry`, validates tooling protocol 1 and pins entry-only sessions. No first-create prompt persistence or TOML overlays exist. Extend snapshots/protocol in the compiler service. |
| [selected seven-job workflow](../.github/workflows/ci-link-experiment.yml) | Native OS/profile matrix and fatal sanitizers are already specified. This assignment changes no workflow and creates no hosted result. |

## Reconciled older contracts

| Older statement | Reconciliation in this assignment |
| --- | --- |
| Bible Section 10 and Stage 8/9 select exact `main.void`; the 12 October discussion recommends only that filename for automatic main. | Keep implemented selection and dated evidence; mark the older future recommendation superseded by filename-independent compiler discovery. Examples keep their existing filenames. |
| The original module phase says there is no automatic discovery. | Label that phase boundary historical and link Stage 8's implemented algorithm plus the unimplemented replacement. |
| Entry parent is always the project VFS root. | Preserve explicit standalone behavior; propose a manifest-owned root independent of entry location only in selected project context. |
| Imported main is never selected. | Preserve explicit-entry behavior and keep libraries/dependencies ineligible. Proposed source-set membership, not merely being imported, determines automatic application candidate eligibility. |
| Local-library package details are unapproved; `lib/raylib` is an example. | Record the new manifest/SemVer/linkage direction while leaving exact schema/resolver/ABI details proposed. Keep the source layout example separate from real C integration. |
| Trusted providers are static; general dynamic-library ownership is deferred. | Preserve working static providers and lifetime evidence; dynamic-default general C support is a new approved direction, not a delivered loader. |
| Numeric types say `as?` is unsupported. | Still true at baseline. Record checked numeric-enum direction separately; both `as?` and `??` need grammar/type review. |
| OS API says exactly thirteen functions. | Preserve the original thirteen-query contract and acknowledge the later implemented `os.args()`/per-Runtime context, making fourteen functions; zero-argument app locations remain future work. |
| Old roadmap/Stage 9 says no package manager ships. | Preserve that sprint's exclusion; distinguish today's private VOIDUP toolchain installer from an unimplemented project dependency resolver. |

## Proposed manifest and identity model

### Initial schema 1: Slice 1 metadata surface

One UTF-8 TOML document named exactly `void.project.toml`, one project and one
application **or** library target. `schema = 1` is a required integer format
revision, independent of the required project SemVer string. No implicit schema,
parent-manifest inheritance, multi-target workspace, environment expansion or
executable hooks. These examples are accepted by the explicit Slice 1 parsing API;
they remain inactive configuration for current voidc/voidls commands. The full
project-aware admission/execution contract below remains future implementation.

Minimal application; neither an entry key nor a filename convention is required:

```toml
schema = 1

[project]
id = "org.example.orbit.sources"
name = "Orbit sources"
version = "1.2.3-beta.1+build.7"
kind = "application"

[application]
id = "org.example.orbit"
```

A library has package identity and no application identity:

```toml
schema = 1

[project]
id = "org.example.geometry.sources"
name = "Geometry sources"
version = "2.0.0"
kind = "library"

[package]
id = "org.example.geometry"
name = "geometry"
```

Optional application fields/settings, merged into the minimal application above:

```toml
[application]
id = "org.example.orbit"
name = "Orbit"
version = "1.2.4+desktop.2"

[sources]
roots = ["src"]
exclude = ["src/experiments"]

[build.debug]
optimization = "none"
debug_info = "full"

[build.release]
optimization = "O2"
debug_info = "line"
```

The blocks are separate examples, not a TOML concatenation that redeclares a table.
If needed, `entry = "tools/launcher.void"` belongs **inside `[project]`**. It is a
file override, never a function selector, and is valid only for an application.

| Initial implementation field | Validation and default |
| --- | --- |
| `schema` | Required integer exactly `1`; bool, float, string, missing and other revisions fail. |
| `project.id`, `project.name`, `project.version`, `project.kind` | All required. Kind is exactly `application` or `library`; no inference from presence of main. Identity/name/version rules below. |
| `project.entry` | Optional portable manifest-relative exact `.void` file, including files outside seed roots/exclusions but inside the project boundary. Lexical errors fail parsing; missing files fail later admission without fallback. Forbidden for a library. |
| `application.id` | Required for application kind; never derived from project/package ID. `[application]` is forbidden for library kind. |
| `application.name`, `application.version` | Default respectively to project name/version; explicit values use the same name/SemVer validators. |
| `package.id`, `package.name` | Required for library kind; package name is a non-keyword compiler-valid lowercase `snake_case` import identifier. `[package]` is forbidden for application kind in this one-target schema. |
| `package.version` | Defaults to project version. Changing package version never changes an application's ID. |
| `sources.roots` | Nonempty array of up to 32 unique, nonoverlapping relative directories; default `["."]`. Directories must exist at later physical admission; Slice 1 validates only lexical policy. Named overlays are later work. Root does not relocate the project VFS boundary. |
| `sources.exclude` | Up to 256 unique relative file/directory paths; default `["tests", "examples", "vendor", "node_modules", "dist"]`. An explicit array replaces this default; `[]` removes these convenience exclusions. Paths may be absent. Exact paths exclude themselves and directory descendants, not similarly prefixed names. No globs/negation. |
| `build.debug.optimization`, `build.release.optimization` | Exactly `none` or `O2`; defaults respectively `none` and `O2`. |
| `build.debug.debug_info`, `build.release.debug_info` | Exactly `none`, `line` or `full`. Both default to `none`. An explicit `line` or `full` in the selected profile reports `VOID5012` because profile-controlled debug information is deferred. |

Manifest-relative paths use `/`, printable portable ASCII components and exact
case, no absolute/drive/UNC form, backslash, empty segment, `.` segment (except the
whole root `.`), `..`, trailing separator, NUL or alias. Preserve the compiler's
reserved-device/trailing-dot/space checks. An exclusion may name a directory
without an extension. Do not trim, normalize case or silently rewrite paths.
Absolute physical checkout ancestors keep the current platform-admission rules;
Unicode physical roots do not make Unicode VFS filenames supported.

Root-level tables `dependencies` and `native` are **reserved for later phases**.
An explicitly empty table is accepted as inert; every member, subtable or value
fails with “project dependencies/native bindings are not supported in schema 1”.
Do not silently ignore or download them. Future versions will specify constraints,
locks, per-target artifact/ABI metadata and dynamic-default/static-optional linkage.
Those declarations need an explicitly supported schema revision. Profiles other
than debug/release, target triples, output/resource settings, custom flags, scripts,
external imports, registry configuration and package artifacts are not schema 1
fields. Unknown keys at every depth fail, including unknown profile names. No
catch-all extension table. Library support initially means source validation and
metadata, not a claimed static/shared library build or packaging command.

### Exact identity and semantic version rules

Project ID, application ID and package ID are distinct **roles**, not aliases.
Each uses the existing portable [OS ID grammar](os/API.md#application-placement):
1–255 ASCII bytes; first/last lowercase letter or digit; interior lowercase letters,
digits, `.`, `_`, `-`; no empty dot pieces; reject the existing Windows device-stem
set on every platform. Reuse one validator specification so `os` and manifest IDs
cannot disagree. IDs may have equal text if explicitly authored, but the compiler
never fills one role from another. A display name cannot be an ID fallback.

Names are strict UTF-8, 1–128 Unicode scalar values, with at least one non-space
scalar and no C0/C1 controls or NUL. Retain authored spelling without normalization;
package import names additionally obey the identifier rule in the field table.
Project IDs persist across relocation; app IDs persist across updates and display
renames. Changing app ID intentionally selects a different storage namespace and
requires restart; it does not migrate or delete old storage. Changing project ID
starts a new project/cache/reload identity. Copying a checkout retains IDs unless
the developer explicitly forks them. Independent processes/Runtimes may use the same project ID without sharing state.
Inside one owning project-analysis/native namespace, two distinct admitted physical
roots claiming one project ID diagnose a collision instead of merging. No process-
global registry is introduced. Project-owned source identity is the pair of stable
project ID and existing canonical VFS/module-symbol identity; visible `/path.void`
keys and the reserved `/@stdlib/` domain retain their current spelling. Native/cache/
reload records carry the project scope in project mode; legacy mode stays unchanged.

Version strings are 1–255 ASCII bytes and exactly
`major.minor.patch[-prerelease][+build]`. Core components are nonnegative decimal
integers with no leading zero except `0`; no `v` prefix, whitespace, signs or
omitted components. Prerelease/build identifiers are nonempty dot-separated
`[0-9A-Za-z-]+`. Pure numeric prerelease identifiers cannot have leading zero;
build identifiers can. Compare core components numerically without machine-integer
overflow (digit-length/lexicographic comparison suffices). A release outranks its
prerelease; prerelease numeric identifiers compare numerically and rank below
nonnumeric ones, nonnumeric ones compare ASCII lexically, and an otherwise equal
shorter list ranks first. Build metadata is retained but ignored for precedence.
Version equality for artifact/build identity still includes exact bytes. Examples:
`1.0.0-alpha.2 < 1.0.0-alpha.10 < 1.0.0`; `1.0.0+a` and `1.0.0+b` have equal
precedence and different metadata identity. Do not implement a dependency solver
or constraint grammar to parse these simple version values.

### Validation, diagnostics and bounded work

Read at most 256 KiB per manifest, including overlays. Reject invalid UTF-8, invalid
TOML, duplicate keys/table redefinition, unknown keys, wrong types and malformed
values. No coercion from integers to strings, bool to schema, or scalar to arrays.
Accept valid TOML quoting/comments; a parser dependency must retain physical byte
spans and duplicate-key sites. Parser/library selection and its license review are
implementation choices, not permission to fetch tools during project analysis.

Retain an immutable manifest SourceFile with UTF-8 byte spans; CLI uses the existing
source diagnostic renderer and voidls converts those same spans to UTF-16. Missing
fields anchor their containing table/EOF; duplicates show both definitions; invalid
paths/IDs point at their value. Suggest the correct supported key or concrete fix
without silently repairing it. Unsupported schema gets a matching-toolchain hint;
reserved declarations get a later-phase explanation. Multiple nested manifests
are separate projects, not duplicate definitions. Wrong-case manifest spellings,
case siblings, symlink/reparse manifests, nonregular files and manifests with a
native link count other than one fail. Source alias checks retain their own policy.
No source error causes fallback to a parent manifest or another application.

Diagnostic categories are schema/key/type/value/identity/path/boundary/entry/
resource-limit. Slice 1 assigns VOID5001–VOID5008 in the existing diagnostic registry and uses
VOID9001 for resource limits; later boundary/entry diagnostics remain future work. Manifest/discovery/source failures are CLI
status 1; unknown commands/options, conflicting selectors and malformed CLI option
usage remain status 2. Application return status retains existing behavior.

Proposed source inventory limits: 4,096 eligible `.void` files, 64 MiB aggregate
captured eligible source bytes, 16,384 visited directory entries and 64 descendant
directory levels. Count named new overlays in the same limits; tombstones remove
their file. Existing parser/import/recursion/diagnostic limits still apply to the
execution graph. Reaching a limit produces a diagnostic and no entry selection;
never truncate the candidate search and call the remaining main unique.

Profiles change optimization/debug metadata, not checks, GC, numeric semantics,
trust or ABI. In project mode choose release by default (preserving default O2),
then profile settings, then explicit `--no-opt`; `--cache` still selects eligible
Debug JIT code and must reject a conflicting effective profile/optimization rather
than silently changing a selected release profile. An implicit default release may
be replaced by `--cache`'s debug selection. `--no-cache` is still run-only and
controls disk caching, not optimization. Keep nonproject command semantics exact.
Project default output is `<manifest-root>/build/<selected-entry-stem>[.exe]`;
explicit `-o` stays caller-relative/absolute, with current collision, staging and
publication checks. No new output-path or target field is required in schema 1.

## Proposed metadata delivery and OS locations

### Immutable application context

A validated application manifest produces an owned context record with metadata
format revision, project ID/name/version and application ID/name/version.
Metadata format revision is separate from TOML schema and SemVer.
The compiler's project analysis retains it; the IR validator checks record shape
and declared startup requirements before LLVM lowering. No borrowed TOML pointers,
Runtime addresses, native handles, source paths or secrets enter the record.

Keep build provenance separate: each NativeCode/generation or AOT artifact carries
its own build identity and the context record associated with that code. Compute
build identity from a versioned input recipe (normalized config, captured source
bytes, compiler/target/profile and verified resource identities), excluding the
derived build-ID field and final binary bytes. This avoids a self-referential hash.
Generation provenance is not the once-only Runtime app context: a compatible body
replacement may change code/build identity while retaining the same app context.
Do not accidentally make every function-body edit a metadata restart.

AOT embeds the record and passes it through the existing application/Runtime
bootstrap **before eager global initializers**. It cannot require the source tree,
project manifest, cwd, environment, argv[0] or online lookup after installation.
Relocation changes executable/resource observations, not embedded application ID.
The ordinary installed Runtime/native resources still come from the matched
compiler inventory. No new trusted sidecar file overrides embedded identity.

JIT passes the same owned validated record to its Runtime before first invocation,
beside the existing application-arguments admission. Admission is once-only,
including an explicitly absent context; incompatible mutation or late admission
fails before entry/globals. Separate Runtimes get independent contexts, never a
process-global current app singleton. Metadata stored in NativeCode/generations
must outlive callers, just as existing retained snapshots do. Metadata-only programs
need not acquire a Runtime merely to carry an immutable embedded record; code that
uses an OS convenience or globals must have context attached before startup. Keep
the current Runtime-free scalar path unless a required operation needs coordination.

An embedding host may explicitly supply a validated context for a standalone/library
consumer. It must match a manifest application's compiled metadata when that exists;
conflicting context is an admission error, not an identity override. Libraries
carry package metadata only and never create/replace application context. Library
code calling `os.data_dir()` observes the consuming Runtime's application ID.

Use the exact application ID grammar in the manifest section. Invalid manifests
fail before execution; invalid host context fails admission. An absent context is
allowed for standalone/embedding use. It is not reconstructed from a project ID,
package ID, process name, bundle or source filename. None of these IDs authenticate
ownership or grant filesystem/native/package-installation authority.

### Four closed call adaptations, not general overloading

The existing public facades are ordinary one-parameter functions; duplicate source
function names diagnose, and parameter defaults are pure constants. A runtime app
ID cannot be supplied by pretending it is an ordinary constant default. **Proposed
freeze:** keep all four existing public one-string declarations and add a finite
compiler-owned zero-argument call adaptation for their exact resolved standard
module declaration identities. Reuse the closed stdlib bridge pattern; do not
change general function declaration, overload or default-argument rules.

After normal import/alias/visibility lookup, a direct zero-argument call to one of
`data_dir`, `config_dir`, `cache_dir`, `state_dir` in the trusted installed `os`
module binds a compiler-verified private facade adapter. That adapter obtains the
Runtime ID and delegates to the **same existing explicit-ID source function**, so
ID validation, placement and native root discovery remain single implementations.
One-argument positional or named `app_id` calls use the original declaration and
evaluation order. Shadowed/project functions with matching spelling gain nothing.
Bridge admission checks actual module/declaration/signature, not raw text alone;
typed IR validates the selected adapter and owner/effect rules independently.

A bare function value `os.data_dir` keeps its existing `func(string) -> result<...>`
type. A zero-argument function-value coercion is unsupported; write an ordinary
zero-argument closure calling `os.data_dir()` if needed. This deliberately limits
the new convenience to direct compiler-bound calls. Hover/signature help shows the
two call forms and the missing-context behavior; navigation retains the real public
facade target, never a fabricated source file for a synthetic adapter.

### Missing context, errors, cache and reload

Each zero-argument getter returns the existing `result<string, os.error>`. Missing
app metadata returns **existing `unavailable` (code 6)**, operation respectively
`os.data_dir`, `os.config_dir`, `os.cache_dir`, `os.state_dir`, empty path/native
domain, zero native code and a fixed value-free message “Application identity is
unavailable.” Do not query platform roots first. Existing explicit invalid IDs
still return `invalid_argument`; ordinary platform failures retain their existing
kinds/codes/domains. Allocation/internal ABI failure is still checked Runtime
failure, not a fabricated ordinary result.

Preserve owner-thread/worker rejection, NativeSafe, precise roots, strict owned
UTF-8 paths, no directory creation, no symlink resolution, no access/ownership
promise and all platform placement rules. Explicit IDs keep working with absent
metadata and may intentionally name another valid namespace; no authentication
system or new permission check is introduced. JIT image/resource/PID queries still
describe voidc/host, while these four convenience calls use explicit app context.

Metadata semantic changes (including name/version), project ID/root/source policy,
entry selection or profile changes require a new project selection and application
restart. Comment-only manifest edits can rebuild diagnostics/cache identity without
mutating a live context. Compatible body-only replacement with unchanged metadata,
execution graph and existing global contract retains context/global cells. Rename
of the entry module changes VFS symbol/global identity and requires restart.

Every project invocation revalidates its manifest and selected source policy before
consulting native cache artifacts. Automatic mode also revalidates the complete
inventory; explicit-entry mode has no D scan and revalidates E instead. Extend the
outer cache identity with compiler/protocol schema,
exact manifest bytes, project ID, canonical physical root, source policy, sorted
eligible membership/source bytes in automatic mode (or an explicit-mode tag),
selected entry, effective profile and metadata bytes, plus existing actual import
snapshots/provider bindings/resource receipts.
This conservative key notices a newly added second main even when the old entry's
bytes did not change. Cache hits receive fresh Runtime context/state for fresh
applications. No live roots, paths-as-authority, callbacks or loader handles are
serialized. Updating project metadata cannot bypass existing provider/hash checks.

## Proposed compiler-owned entry discovery

### Project boundary and precedence

New discovery is **manifest-selected only**. The prior broad suggestion to scan
manifestless directories for arbitrary main is deferred to preserve existing
invocations. `main.void` has no preference inside a manifest project. Without a
selected manifest, the exact historical helper and standalone behavior remain.

1. A bare explicit `.void` CLI operand always uses legacy explicit-file semantics:
   entry parent is VFS root, no manifest lookup/context, even beside a manifest.
2. Proposed `--project <directory>` requires exactly that directory's manifest;
   no source/directory positional selector may accompany it. Optional `--entry`
   is manifest-relative, overrides `project.entry` and cannot escape/cross a nested
   project. An invalid CLI override never falls back to the manifest override.
3. A directory operand, or cwd for absent run/build operands, uses its exact manifest
   if present. A malformed/wrong-case/aliased manifest fails closed. If absent,
   call the existing directory-selection helper unchanged; do not search ancestors.
4. The editor with a selected project-root setting pins that manifest. With only
   legacy `void.project.entry`, preserve legacy explicit-file behavior. With neither,
   walk active-file ancestors within the one selected workspace, nearest manifest
   first; without an active file inspect the workspace root only. If no manifest,
   call existing `discover_project(workspace, active, override)`. No search above
   workspace, into sibling projects, home or an unrelated cwd.
5. A nested exact manifest marks a child project; a parent inventory never descends
   into it. Parent-context imports/overrides may not cross that boundary. Selecting
   the child deliberately makes it the root. A malformed child manifest still marks
   a boundary; it is not permission to absorb its sources. A wrong-case/colliding
   manifest encountered inside a traversed directory diagnoses. Skipped/excluded
   subtrees are not opened just to hunt for errors.

Inspect paths with the existing exact-case and alias rules before accepting a
boundary. An explicit pinned root plus an active file in a child/outside project
diagnoses an out-of-project document, rather than silently choosing another root.
No manifest merging, fallback after errors or workspace-wide project guessing.

### Eligible source set and deterministic selection

Let **D** be the discovery inventory, **E** the selected execution import graph.
The frontend owns both. Schema 1 roots enumerate descendants recursively; apply
exclusions first, then admit exact `.void` source names through shared loader path
checks. `.git`, `.void`, `build` and `libs` path components are always ineligible
for automatic seeds; stdlib and future external dependencies are never seeds.
An explicitly listed root cannot promote these reserved components. Other default
exclusions can be changed with `sources.exclude`. Exclusions restrict **D**, not
ordinary in-root imports; an imported excluded file still passes all loader and
nested-boundary checks. A present `.VOID` file is not a source; a case sibling of
an admitted `.void` identity still diagnoses through the existing collision policy.

Named unsaved `.void` overlays inside admitted roots are inventory members; exact
canonical tombstones suppress disk files, and dirty content wins over disk. Virtual
parents are allowed under admitted physical roots by the existing overlay rules.
Untitled/external/installed-standard documents cannot claim project identities.
Reject eligible symlink/reparse files/directories and hard-link identity collisions,
including overlay-over-host aliases; do not follow a candidate link to inspect it.
Enumeration failure, unreadable admitted source or exceeded work limit fails the
selection, never a partial successful search. Sort by case-sensitive canonical VFS
identity using UTF-8 byte order, then source span, before parsing/reporting.

Parse **D once per snapshot through the existing lexer/Parser**, retaining Units
and raw entry/header metadata. This requires separating parsing from SourceGraph's
current recursive import loading, not a second parser. Parse errors anywhere in D
prevent a successful selection. Non-entry helpers outside E are not semantically
validated or initialized merely because discovery parsed them. This refines the
original proposal to validate every application seed: it preserves the established
“unimported globals do not initialize” and reachable-analysis contracts.

A candidate is a top-level ordinary non-generic, non-receiver, synchronous
`func main()` with zero parameters and either no result or exact `i32`. Defaulted
parameters still count as parameters. Visibility may be public/internal/private,
matching current explicit-main admission. A top-level free main with a wrong
signature is an entry diagnostic, not a silently ignored alternative. Methods,
nested functions and a tagged variant named main are not candidates. A main in a
library-only path/import is ordinary source; import reachability alone never
promotes it into D. Conversely, a file already in D stays entry-eligible even if
another application seed imports it. To mark such a helper library-only, put it
under `libs/`, outside roots or in an explicit exclusion. Do not infer source roles
from graph indegree or cyclic imports.

For an application without an override: zero candidates is an error **also for
project check**; one selects that source; two or more diagnose ambiguity, with
canonical sorted related locations and a `project.entry`/`--entry` remedy. A file
named main.void never wins a tie. Diagnostic output may summarize a very large
candidate list with its full count and an explicit truncation notice, but selection
must consider every member within the work limits. Body/type errors in the unique
candidate fail subsequent validation, with no attempt to choose a different main.

An explicit `project.entry`/`--entry` bypasses uniqueness scanning: load its source
and E only (no unrelated seed syntax errors), then apply **existing** main/script/
legacy-entry/declarations-only classification and legacy precedence. Validate the
manifest/source policy itself and the explicit path even when D is bypassed.
Automatic main discovery does not search for script bodies or legacy entries. If
the unique main's source also declares legacy entry, retain existing legacy
precedence and argument/status behavior; typed validation still rejects competing
main/script forms according to existing rules. No function-name entry selector.

After selecting the entry, build **E as its import closure**, reusing any captured
D Units, and use the existing semantic validator, root visibility, global ordering,
independent typed IR gate and shared LLVM lowering. There is no discovery-only
initializer execution. Unimported D files are not emitted or made root-visible.
The **selected entry module**, even `/src/launcher.void`, owns root public/internal
constants/globals. The manifest directory fixes VFS `/`; it is not an implicit
module. Imported module private/internal/public rules and stdlib isolation remain.

For library kind, do not select main at all. Check the union of eligible library
source roots and their imports with ordinary module visibility and a declarationless
root scope. This requires making the frontend root environment explicit; do not
pretend an arbitrary library file is an application root or invent an importable
synthetic source module. All executable top-level statements in that graph reject;
functions named main are ordinary helpers. Check is valid with no main or even an
empty source set. No startup or IR executable entry is fabricated. Project run,
build and emit diagnose that library artifacts/execution are unsupported in this
initial delivery; explicit standalone file commands remain separately available.

### Snapshot stability and session updates

Freeze manifest bytes, admitted path identities, D membership/bytes when used,
overlays, entry result and E snapshots as one selection revision. Include observed
manifest boundary presence/absence along inventoried/imported ancestors, so a new
child boundary cannot hide behind unchanged source bytes. Rediscover/recompare
the applicable disk membership and captured bytes before accepting a project result; concurrent change
produces “Project changed during discovery; retry” rather than stale uniqueness.
This is detection/retry, not an OS filesystem transaction. Cache hits cannot skip
this gate. No partial/failed discovery becomes `ValidatedAnalysis` or authorizes IR.

Edits within a pinned project's unchanged boundary can reanalyze/recompute candidates
and publish new diagnostics against frozen overlays. Entry/source-policy/metadata
changes mark the current executable selection as needing explicit reselection or
restart; they never publish code or rebase dirty buffers silently. Changing root
requires deliberate selection and a new session; manifest dirty edits may preview
errors but cannot automatically adopt another root. CLI build/run uses disk bytes;
LSP uses its retained unsaved snapshot. Parity means identical inputs/revisions,
not pretending a disk command sees unsaved editor changes.

Represent named TOML overlays separately from `.void` SourceOverrides. A new/deleted
nested manifest overlay changes boundary membership for editor previews, even if
its contents are invalid; it never silently selects that child as the session root.
A selected-manifest tombstone/absence invalidates a pinned project session instead
of falling back to legacy discovery. CLI disk snapshots remain separate. Creation
and nested-manifest watchers must therefore invalidate boundary observations as
well as source bytes, with the same revision/stale-result guards.

## Phase 1 compatibility and migration matrix

All “new” behavior below is proposed, not shipped. No source rename is a migration
requirement; adding a manifest is the explicit opt-in that can expose new ambiguity.

| Invocation or case | Baseline | Proposed initial project contract |
| --- | --- | --- |
| `voidc run app.void`, including beside a manifest | Explicit entry; parent root | Unchanged; no implicit app metadata. Use project context to get manifest metadata/root. |
| Directory/cwd with no manifest | Exact main.void discovery; current missing-main failure | Unchanged; no filename-independent directory scan yet. Explicit file still works. |
| Directory/cwd with exact valid manifest | Manifest ignored | Opt in to schema, manifest root, kind and new discovery. Release/O2 default retained. |
| `--project DIR [--entry PATH]` | Unknown option | Explicit manifest context; path override wins without changing VFS root. No positional file selector. |
| Legacy `void.project.entry` alone / old `voidls --entry FILE` | Fixed explicit source session | Unchanged legacy session. New project-root setting plus entry is an explicit project override. |
| Any-name single main in manifest source set | Directory still expects main.void | Select main's source irrespective of name; renaming causes reselection/restart, not application-ID change. |
| Multiple candidates, even with main.void | Old discovery chooses main.void | Ambiguity; add an explicit override or correct source policy. No preference. |
| No main in application manifest / library manifest | Explicit check allows declarations-only files | Project application check fails without override; library check succeeds without entry. Bare explicit check remains unchanged. |
| Explicit script or declarations-only file | Existing top-level order / globals initialize then zero exit | Unchanged, including project override; automatic discovery does not guess scripts. |
| Imported library main | Ordinary helper | Ordinary helper unless its file is independently an eligible application seed. |
| Root constants/globals | Selected entry module owns root scope | Same logical owner; manifest root controls VFS paths, not declaration visibility. Only E initializes for apps. |
| Local `libs/` / stdlib | Project-local domain / reserved stdlib domain | Same identities and visibility; no dependency/provider admission inferred. |
| Installed compiler and native receipts | Executable-relative matched resources and explicit stdlib override | Same resource/SDK/hash checks, independent of source manifest. No ambient fallback. |
| Old source/JIT caches and reload | Entry/graph/ABI/bytes identity | Project key is distinct; old key cannot supply missing project validation/context. Existing legacy path unchanged. |
| Actual application arguments | Later VOIDUP supports explicit source plus `--` forwarding | Preserve that grammar and literal tokens, plus an explicit `--project` context can own the forwarding separator. Original Stage 9 no-operands wording is historical for this later extension. |

To migrate a root-relative import from bare `/helper.void` under `src/` to a manifest
root above it, deliberately update it to `/src/helper.void`, or retain an importer-
relative spelling. Report the selected root in CLI/editor diagnostics; never
rewrite source imports or silently reuse the old entry-parent VFS root. Existing
one-file/standalone workflows need no migration.

## Phase 1 ownership and data flow

Names here describe proposed records/responsibilities, not current public APIs.

| Owner | Input → output and obligations |
| --- | --- |
| Frontend project loader | Admitted explicit root + immutable TOML snapshot → validated `ProjectConfig` with spans, normalized field values, kind and source policy. No network, native load or execution. |
| Frontend selection | ProjectConfig + frozen overlays → `ProjectSelectionSnapshot` holding D membership/bytes, candidates, selected entry or diagnostics; library kind has no entry. Preserve path checks from SourceGraph. |
| Frontend semantic analysis | Selected entry/E (or library check graph) → owning successful analysis only after all validation gates. Root path and root declaration owner are separate inputs. Existing `analyze_project(entry)` remains a legacy adapter. |
| Typed IR / LLVM | Successful analysis + metadata/startup requirements → independently validated IR, then existing normal/O2 JIT or AOT lowering. Preserve exact adapters, failure ABI, GC and leases. |
| Runtime/bootstrap | Owned immutable app metadata + args → once-only admission before globals, reused by closed OS adapters. Library records cannot install identity. |
| CLI/native builder | Literal operands/options → shared selection, profile and existing verified resource/link plan. Project metadata does not replace resource/provider receipts. `-o` collision/publication policy unchanged. |
| ToolingSession / voidls | Pinned root + source/manifest revisions → shared compiler results, UTF-16 diagnostics/navigation, stale-result rejection. Error-bearing project sessions are allowed; failed state never supplies native analysis. |
| VS Code adapter | Trust + explicit selection + file events → preflight/session requests, UI and exclusive manifest creation after accept. It neither parses main candidates nor duplicates manifest/VFS resolution. |
| VOIDUP | Existing admitted toolchain selection/inventory → installed compiler/server/resources. No schema 1 project dependency install or resolver ownership is added. |

Flow: disk/overlays → project config → discovery snapshot → selected E → validated
analysis → typed IR → JIT/AOT bootstrap → immutable Runtime context → existing OS
placement facade. Diagnostic/session queries stop before code generation/execution.

Proposed separate preflight: `voidls --discover-project WORKSPACE ACTIVE ROOT ENTRY`
uses empty strings for absent inputs and returns a schema-2 bounded JSON summary
(mode, root, manifest, kind, nullable entry, selection revision, status and diagnostic
summary). This is a new endpoint; old `--discover` keeps its exact three-argument
inputs/JSON and legacy semantics. Preflight is no guarantee that disk stays stable.
Domain errors such as missing main return the error-bearing summary; malformed
invocation/native launch failure remains a process error. Keep summary within the
existing adapter's 64 KiB buffer; detailed source diagnostics belong to the session.
For first-file offers, the compiler may additionally perform an inventory-only
query of the selected workspace root using default source policy when no manifest
exists. Return `creation.root`, exact eligible count and all identities when there
are at most 16 and they fit the summary; otherwise mark creation detail incomplete
and suppress the automatic offer. No source parse, main selection or manifestless
CLI behavior change follows from this creation-only inventory.

New `voidls --project DIR [--entry PATH]` starts a pinned manifest session, including
when main is missing or TOML is invalid. Invalid TOML permits manifest diagnostics
but no traversal based on invalid policy. Extend ToolingSession snapshots to carry
manifest overlays and inventories; no fake `.void` identity for TOML. Add a separate
`voidProjectProtocol: 1` capability while retaining `voidToolingProtocol: 1`; old
adapters/server legacy mode continue unchanged. A new adapter must report missing
project capability clearly instead of pretending old entry-only startup retained
manifest root/metadata. Syntax assistance may use compiler recovery; it never turns
invalid project state into successful ValidatedAnalysis.

Proposed CLI common selectors: `--project DIR`, `--entry PATH`, `--profile debug|release`.
Entry/profile require a manifest-selected context; `--project` and a positional
selector conflict. Directory/cwd implicit contexts may use entry/profile. An explicit
source cannot acquire project semantics from `--profile`. Source/directory operands
and required `-o` rules for check/emit otherwise remain; `--project` itself satisfies
their explicit project-selector requirement. For `run --project DIR --
...`, all subsequent tokens are literal application arguments; keep current legacy
numeric adapter rules if the selected explicit entry is legacy. Preserve early
`--app` rejection only outside the existing literal forwarded region.
CLI `--entry` on library kind is a usage conflict (2), whereas forbidden
`project.entry` authored in a library manifest is a manifest error (1). Invalid
profile spelling or selector combinations are usage errors; missing project/entry
files, invalid configured paths and source failures remain operational errors (1).

No project operation grants authority from configuration. Analysis does not create
storage, execute installers, load a native provider or mutate installed toolchains.
Run/build may consume only already admitted provider/resource plans under existing
host policy. Reserved dependency data, absolute paths, hashes and editor trust alone
are not native loading or package installation authorization.

## Proposed first-file creation and terminal workflows

### Terminal contract proposal

`voidc init [DIR]` uses cwd by default and requires an existing admitted directory.
`voidc new PATH` requires an absent destination under an existing admitted parent.
Both create only project-owned new objects; no downloads, native loads, dependency
installation, PATH/profile changes, source moves or recursive cleanup of existing
content. New command flags are proposed, not accepted baseline CLI grammar.

Shared options: `--kind application|library` (default application), `--name TEXT`,
`--version SEMVER` (default `0.1.0`), `--project-id ID`, plus `--app-id ID` for an app
or `--package-id ID --package-name IDENTIFIER` for a library, and `--nested` for
deliberate child-project creation. `--source` is new-only; using it with init is a
usage error. Reject wrong-kind,
duplicate and unknown options. Name defaults to destination basename if valid;
package name defaults to that basename only if compiler-valid. Otherwise require
an explicit value, with a useful diagnostic. No required interactive stdin session:
complete options and valid defaults work in terminals, scripts and editor calls.

Omitted project ID is `local.p` plus 32 lowercase hex characters from a fresh
128-bit random value. An omitted application/package ID is generated independently
with `local.a`/`local.l` prefix and a second value; never copy one role into another.
Random generation is collision avoidance, not authentication. If the host cannot
supply entropy, fail before publication. Write the values once; init never rotates
IDs because a folder/name/version changed. No global ID registry or implicit app
namespace claim is created. Users can supply stable publisher-owned IDs explicitly.

Init writes a minimal manifest with default roots `["."]` and **no entry field**;
it creates no source or source directory and never renames `game.void`. It may
succeed with zero/multiple main candidates, reporting the next check/discovery
issue without inventing an override. New creates `src/` and `src/app.void` by
convenience for an application, with roots `["src"]`; `--source PATH` may choose
another portable in-root path ending `.void`, setting the sole source root to that
file's parent (`["."]` for `game.void`, `["tools"]` for `tools/launcher.void`).
Reject a source under a reserved seed component before creation. A library template uses `src/<package-name>.void`
with an ordinary declaration and no main. No template name is required by discovery.
New's source option is not written as a mandatory entry override.

Before writing, use the same config/identity/path validators and show resulting
absolute manifest path, generated IDs and next project command. Create the manifest
exclusively; preserve any existing manifest, including invalid/wrong-case/aliased
ones, with a clear collision error. Init does not adopt an ancestor project or
create a child inside it silently: diagnose an enclosing manifest and require an
explicit `--nested` to create an intentional new boundary. New uses the same rule.
`--nested` never bypasses containment/trust or overwrites a child project.

New stages only its own fresh directory content and publishes without replacement;
init stages its own file and publishes without replacement. On error, remove only
objects still proved to be owned by that operation; retain/report an uncertain
publication rather than deleting unrelated content. A successful exclusive manifest
publication may leave an application awaiting source fixes, not a falsely successful
build. Permission, race, output-lock and invalid-name failures remain actionable.
Creation commands return 0 for completed creation, 1 for filesystem/validation
failure, 2 for malformed CLI usage; ordinary app/check rules remain unchanged.

### VS Code first-file state machine

Use only a trusted supported single filesystem workspace. On `.void` creation or
first save of a new named buffer, ask compiler preflight for enclosing manifest and
eligible source inventory in the selected workspace root (the default creation
destination). Require complete creation detail; larger/incomplete batches use the
manual command. No offer on merely opening an existing file, activation,
untitled/installed/dependency documents, or a later source in an established tree.
If the current new source is the only eligible source, offer once. For simultaneous
first sources, coalesce event revisions and allow one offer only if every source in
the resulting nonempty inventory belongs to that fresh event batch. Existing
unrelated sources suppress the offer. This resolves the concurrency race without
extension-owned source enumeration.

Prompt key: schema-independent canonical workspace/root physical identity in
extension `workspaceState`, with values `unseen`, `offered`, `accepted`, `declined`,
`dismissed`. Write `offered` **before** showing the prompt; if the host closes then,
treat it as suppressed on reopen. Both **Keep standalone** and UI dismissal persist
suppression. Accept records `accepted` even if a later preview is cancelled or a
write fails; a deliberate **VOID: Create Project** retry is still available. No
manifest deletion, second file, restart, version upgrade or trust toggle resets
suppression. **VOID: Reset Project Creation Prompt** explicitly resets that root;
its next qualifying event or the Create Project command can offer again. Moving
to a different physical workspace is a different prompt key, not portable memory.

Accept opens metadata/destination review using the same init contract and displays
project/app/package IDs; only acceptance plus completed review writes. Use an
exclusive create and recheck root/manifest after races. If another actor created a
manifest, preserve it and ask shared discovery to select it. Failure reports once
and leaves the source/standalone analysis usable. Decline/dismiss creates no files
or source-controlled suppression marker. Never name a manifest `main.void` or rename
an existing file. The Create Project command can initialize an existing multi-file
root deliberately; the automatic offer is specifically for the first-file event.

An editor with auto-selected manifest uses new project protocol. Add literal setting
`void.project.root`; alone it pins a manifest root, and with `void.project.entry` it
selects an in-root override. A legacy entry setting without root remains legacy.
The explicit Select Project Entry command in project mode pins the root together
with the chosen entry; it does not silently move the VFS boundary. No filesystem
write, compiler spawn, dependency install or native load in an untrusted workspace.
The existing configured/bundled/PATH server selection and supported-workspace limits
are retained. Creation UI is separate from compiler/LSP semantic authority.

## Proposed numeric-backed enums and fallible conversion

The S6 design approval resolves this section's earlier alternatives for the
implemented family. `enum mode(u8):` declares an exact nominal type with explicit
decimal discriminants. Integer `as?` returns canonical `option<T>` after range and,
where applicable, discriminant membership checks. `Some` and `None` use existing
tagged-sum storage and matching. The approved slice defers `??`, nullable casts,
enum extraction and C ABI rules. See the [language contract](NUMERIC_TYPES.md#numeric-backed-enums-and-fallible-casts)
and [Slice 6 qualification](projects/SLICE6.md).

### Historical proposal

Introduce a separately specified numeric-backed, payload-free enum form with one
explicit fixed-width integer representation and declared discriminants. Do not
reinterpret existing tagged payload variants or expose their internal tags as C
integers. Declaration syntax, implicit numbering, aliases/duplicates and enum-to-int
conversion are open. Recommend rejecting duplicate discriminants initially; do not
assume contiguous values or treat flag bit combinations as ordinary enum members.
Keep current standard-library error enums and public error codes stable unless
their migration is separately reviewed.

The requested convenience is **illustrative, not executable syntax**:

```text
var kind = (code as? error_kind) ?? error_kind.system_failure
```

`as?`, nullable enum representation, `??` precedence/associativity and fallback
typing all require lexer/parser/type-checker verification. Proposed semantic shape:
integer-to-numeric-enum conversion returns an optional exact nominal enum; invalid
representation or an undeclared discriminant returns absence. If nullable enums
do not fit existing contracts, review an explicit result-returning API instead of
inventing a nullable stand-in. Decide this before publishing source syntax.

Evaluate the source once; first check mathematical representability in the backing
integer without truncation/wrapping, then exact membership in the declared set.
Negative-to-unsigned, out-of-range and holes cannot create an invalid enum. The
fallback is lazy and evaluated once only on absence. Invalid types/payload enums
diagnose statically; a reached invalid constant has the same absence semantics as
a runtime operand, subject to the chosen result shape. Existing checked numeric
`as`, float behavior, RuntimeFailure and cleanup contracts do not change. Floats,
bools, arbitrary bitcasts and C unknown enum values gain no implicit conversion.

Use one semantic specification for constant evaluation, typed AST, independently
validated IR and shared LLVM normal/O2 JIT/AOT. Guard values before constructing
enum IR, switches or payload access so optimizer assumptions never admit invalid
tags. Representation/discriminant changes alter ABI/reload/cache compatibility.
C bindings preserve raw integer error values when unknown C enum values are legal;
checked conversion is explicit at the safe wrapper boundary.

## Proposed general-purpose C interoperability

No raylib compiler subsystem. Raylib bindings are ordinary typed declarations and
VOID wrappers over a reusable trusted C ABI facility. Source declaration grammar
or generated-binding format remains open. A bounded, auditable binding generator
may consume C headers offline using the matching toolchain; no second VOID parser
or runtime compilation of arbitrary headers is needed.

| C surface | Proposed contract and review gate |
| --- | --- |
| Functions | Exact external symbol, typed parameters/results, explicit target C calling convention, blocking/thread/callback effects, error mapping and ownership. Reject unsupported variadics initially or use an audited fixed-signature shim. |
| Structs | Verify size, alignment, field offsets and by-value argument/return classification per target; do not equate a VOID managed struct with a C struct. Use marshalling/value storage with precise GC metadata and opaque owned resources for retained pointers. Packed layouts, unions and bitfields require explicit support or rejection. |
| Enums/constants | Bind exact target integer representation and evaluated typed constant values. C macros are not executable VOID declarations; unsupported function-like macros need wrappers. C enums with unknown values remain raw integers until checked. |
| Strings/buffers | Explicit UTF-8/NUL policy, length, nullability, copy/borrow/retain duration and release routine. No pointers into movable/temporary managed storage; retain precise roots and existing NativeSafe transitions. |
| Callbacks | Exact callback C ABI, result/failure translation, Runtime/thread affinity, retained captures, provider registration, code and library leases. No C++ exception or VOID failure unwinds across C. |

**Dynamic by default:** select verified target shared objects (`.so`, `.dylib`, DLL)
from an explicit admitted dependency plan, including the transitive closure. AOT
may link their import interface and uses an intentionally packaged loader layout;
JIT explicitly loads and resolves that same verified ABI. Hash/version/architecture,
required symbol and layout mismatches fail before calls. Never load a library to
answer check, completion or hover. ELF SONAME/RUNPATH, macOS install-name/rpath and
signing, and Windows import libraries/DLL search restrictions each need native
packaging proof. File extensions or upstream semantic versions alone prove no ABI.

**Static optional:** explicitly select matching archives (`.a` or platform static
`.lib`), record closure/link order and required system libraries, PIC/CRT/toolchain
compatibility and redistribution obligations. Distinguish a Windows import library
from a static implementation archive. AOT uses the shared native build plan. Static
JIT needs an explicit tested archive materialization/link strategy or a clear
unsupported diagnostic; never silently switch linkage to make a test pass.

Discovery must avoid cwd, broad PATH and ambient loader-variable fallback. An
absolute path alone proves neither provenance nor safe ancestry. Reuse held-file,
inventory and namespace-verification foundations within a documented threat model;
verify the actual selected artifact and its dependencies before admitting code.
Loading can execute native initialization, so a manifest or source import must
never authorize loading implicitly. Explicit run/build requests can consume a
previously admitted plan under host policy; editor trust alone is not native
dependency admission. Separate missing, unverified, incompatible and denied states.
System-library allowlists and loader substitution/race guarantees remain open.

Dynamic library ownership joins Runtime resources and JIT generation leases.
Unregister callbacks at the provider, quiesce pre-admission dispatches, drain
admitted callbacks/jobs and retained resources, then release roots, code leases
and library handles in safe order. Compatible reload retains the old code/library
generation until all users retire; signature/layout/provider changes require
restart. Preserve current restrictions on stored/forwarded callback provenance
until a generation-carrying representation is approved. Never serialize pointers,
loader handles, native state or active registrations in caches/packages.

Raylib acceptance should exercise a real window/draw/input/close lifecycle,
representative by-value structs, enum/constants, owned resource destruction and
the applicable callback/thread contract through those generic facilities. Test
missing library, wrong version/ABI/architecture and invalid discriminants. Record
display/headless limits honestly. Qualify dynamic normal/O2 JIT and linked AOT on
each supported native host; static is a separate explicit gate. Pin the chosen
raylib version and binding revision during implementation, not in this proposal.

## Proposed dependency ownership and VOIDUP integration

VOIDUP continues to select/install/verify compiler toolchains using its own
authenticated catalog and explicit filesystem/process capabilities. A project
resolver consumes manifest requirements and produces a lockfile/admitted plan;
its command owner (`voidc`, a separate package command or a bounded VOIDUP
extension) remains open. **A project manifest is not a VOIDUP release catalog,
`void-native.txt` or an authentication receipt.**

Future package installation must be an explicit workflow: resolve without execution,
show exact source/native artifacts and target, verify provenance/content through
the applicable trust policy, install through admitted capabilities, then build.
Reuse existing verification primitives without copying installation authority
from metadata. No unrestricted package install/build scripts. Preserve Git-based
source direction, immutable locks, offline locked builds and deterministic conflict
diagnostics. Package sources and native artifacts need separate licenses/notices
and matching source materials where required. Compiler toolchain versions, package
versions and C ABI revisions must not be collapsed into one version field.

## Proposed IDE and public usability milestones

Compiler-owned project analysis supplies manifest spans/diagnostics, selected
target/root/entry candidates, app/package identity, dependency state and binding
symbols. Reuse `ValidatedAnalysis`, `ToolingSession`, `ToolingIndex` and voidls;
extend the discovery protocol explicitly rather than altering existing JSON/session
semantics silently. Manifest edits, source creation/deletion and dependency changes
invalidate the appropriate selection/analysis revision and offer deliberate restart.

VS Code provides the first-file offer and Create/Validate Project commands, manifest
field assistance, candidate ambiguity locations, identity/dependency hover and
compiler-owned enum conversion diagnostics. FFI symbols provide completion,
signature/ownership/thread hover and navigation to actual binding declarations or
available source, with null targets for synthetic builtins. Missing raylib artifacts
report target, expected identity and a concrete explicit resolution action; they
never trigger a download. Other adapters consume the same analysis.

Build/run tasks select project, profile and verified dependency plan with literal
arguments and normal output/error reporting. Debug support needs emitted debug
information, platform debugger integration, launch/attach and breakpoint evidence;
an LSP handshake is not debugger acceptance. Surface standalone/project selection
clearly and preserve dirty-buffer/version checks and current workspace limitations.

Publish future guides/examples for install → new/init → arbitrary filename main →
dependency resolution → raylib run → Release package. Packaging commands are still
unspecified. Packages must include application metadata, assets, exact native
closure and notices, use validated platform loader layouts, and run without the
checkout/compiler at runtime. Qualify compiler installation, dependency resolution,
native packaging and editor/debug usability separately. Preserve accepted hosted
installation evidence and existing [release limits](release/PREVIEW_BLOCKERS.md);
this plan neither reopens completed gates nor clears redistribution/publication.

## Phase 1 concrete acceptance matrix

**Test design only. No fixture, implementation, compiler/editor run or new CI result
was produced by this architecture assignment.** Each row is an independent temporary
project unless specified. Use the minimal application manifest above, roots `["src"]`,
ID `org.example.orbit`, and this body for a valid candidate:

```text
func main() -> i32:
    return 23
```

Expected executable status is 23 without automatic numeric printing. `check`/`build`
success is status 0. Harnesses must distinguish app status from compiler status.
Reuse existing source/error/provider fixtures rather than copying a second runtime
harness. Source examples here specify expected future behavior, not runnable tests.

| ID | Concrete input/action | Expected result |
| --- | --- | --- |
| P01 | Only `src/app.void` has the body above; run/check project without entry key | One candidate `/src/app.void`; check/build succeed, JIT and executable return 23. |
| P02 | Rename P01 source to `src/program.void`, then `game.void`, then `launcher.void` | Each name is discovered without manifest edits. App ID/storage path stays the same; entry symbol/cache key changes and reload requires restart. |
| P03 | Only `src/features/launch.void` has main | Select `/src/features/launch.void`; `/` remains manifest directory, not `features/`. |
| P04 | Both `src/main.void` and `src/app.void` have valid main | Ambiguity, CLI 1, both locations in deterministic order; no arbitrary main.void priority or execution. |
| P05 | Only helpers/globals in an application manifest, no override | Project check/run/build/emit fail missing-main; standalone `check src/helpers.void` still succeeds. |
| P06 | Library manifest with only `src/geometry.void` ordinary declarations, then an empty root | Check succeeds without main or startup. Project run/build/emit give explicit unsupported-library-mode errors. |
| P07 | App imports `lib/helper` from `libs/helper/helper.void`, which declares main returning 99 | Only application seed main selected; helper main is not executed. Same result with an excluded imported helper path. |
| P08 | Two files inside D declare main, one imports the other | Still ambiguity; imports do not silently turn an independently eligible app source into library-only. |
| P09 | One valid main plus free `main(value: i32)`, async/generic main or wrong result in another eligible file | Entry-signature diagnostics, not fallback. A receiver method/nested main alone does not qualify. |
| P10 | Unique main body has a type error / parse error in any admitted D source | Source-located failure, no successful analysis/IR or selection of a different file. |
| P11 | Only helper body outside E has a semantic type error, no syntax error or main | Automatic selection remains valid; it is discovery-only. Importing it into E then exposes its error. |
| P12 | Unimported D module has a global initializer with an observable effect | Parse it but do not initialize or emit it; importing it into E runs it once under existing ordering. |
| P13 | Manifest `project.entry = "tools/assets.void"` outside roots; top-level script without main | Override succeeds under existing script classification/order. Invalid/missing override fails with no D fallback. |
| P14 | Two D mains plus explicit `--entry src/app.void`; unrelated D syntax error | Explicit override analyzes only E and executes selected file; no unrelated ambiguity/syntax diagnostic. Wrong override still fails. |
| P15 | Explicit file without manifest, beside a manifest, legacy entry, global-only and top-level fixtures | Replay current explicit-file results/stdout/stderr/status and parent VFS root byte-for-byte. No implicit metadata. |
| P16 | No-manifest directory with main.void; then only program.void | First preserves existing discovery; second retains missing-main failure; explicit program.void works. |
| P17 | Main file also has legacy entry; explicit main/script conflict; imported executable statements | Preserve established legacy precedence/numeric output, conflict diagnostics and imported-script rejection. |
| P18 | Root constants/globals in `/src/app.void`, ordinary imported module and unrelated D module | Existing entry-root visibility only; no root constants from unrelated seeds or manifest; existing private/stdlib isolation. |
| P19 | Parent project plus `src/child/void.project.toml` and its main | Parent skips child; selecting child chooses its own main; parent import/override across boundary fails. Invalid child TOML remains a boundary. |
| P20 | Missing schema/ID/kind, schema bool/2, wrong field type, duplicate/redefined table, typo key or nonempty reserved native/dependencies | Reject at relevant TOML byte/UTF-16 span with actionable category/hint; no downloads, loads or successful analysis. |
| P21 | Library contains application/entry, app contains package, wrong-case/colliding/symlink manifest | Reject forbidden/conflicting configuration or path identity. Valid nested manifests do not conflict or inherit fields. |
| P22 | Invalid IDs `Bad.ID`, `a..b`, `con`, separator/NUL; empty/control name | Shared ID/name validation fails before startup; no filesystem authority or silent ID derivation. |
| P23 | Versions `1.2.3-rc.2+build.07`, `1.2.3-rc.10`; invalid `1.2`, `01.2.3`, `1.0.0-01`, `1.0.0+` | Exact acceptance/order rules; build leading zero accepted, prerelease numeric leading zero rejected; long components cannot overflow. |
| P24 | Roots/exclusions overlap, escape or wrong-case; eligible symlink/reparse/hardlink; exceeded inventory bound | Clear error without following aliases or reporting partial uniqueness. Existing acceptable physical ancestor aliases stay scoped to current policy. |
| P25 | Set exclude `src/scratch`; put second main there, then import it | No second seed candidate. Actual import receives ordinary semantic/safety checks and does not promote its main. |
| P26 | Named overlay adds/renames a second main; tombstone removes it; dirty source changes signature; TOML overlay adds/deletes a boundary | Results follow frozen source/manifest overlay membership/bytes; deleting selected TOML invalidates pinned project mode; same snapshots give identical compiler/LSP results, with CLI parity after save. |
| P27 | Disk adds a candidate or child manifest during capture / older analysis completes after new revision | Retry/diagnostic for unstable capture, including boundary-only changes; stale result not published. No filesystem-transaction claim. |
| P28 | Cached project, add second main without changing old entry; then change manifest/profile/ID | No stale cache selection; ambiguity/metadata/profile keys revalidated. Legacy cache path remains distinct. |
| P29 | App version/display name changes, app ID retained | Fresh context/restart and changed build metadata; all four OS paths retain the same app component. No automatic storage migration. |
| P30 | AOT app moved to another installation, original checkout/TOML/compiler unavailable | Four no-ID calls use embedded ID. Image/resource observations follow existing platform process layout, with no source fallback. |
| P31 | Two manifest apps with distinct IDs in separate Runtimes; library facade called by each | Paths reflect each consuming Runtime's ID; no singleton leakage or library identity substitution. Closing one does not affect the other. |
| P32 | Missing context, zero-argument getter; explicit valid/invalid ID calls | Missing gets code 6, exact operation/fixed message, empty path/domain and native code 0 before OS query. Explicit forms retain current success/error behavior, invalid ID code 2. |
| P33 | Late/duplicate/conflicting metadata admission; compatible body reload; metadata-change reload | Fail invalid admission before startup; body-compatible reload can change generation/build provenance while retaining app context/globals; changed context metadata requires restart. |
| P34 | Zero-argument qualified/import-aliased OS call, named one-string call, shadowed user function, function-value reference | Only exact resolved stdlib direct calls get adaptation; explicit calls and existing one-string callable type remain; zero-arg coercion fails; no general overload support. |
| P35 | Zero-ID OS calls from eager globals/worker-reachable code; GC/allocation fault injection | Context available before globals; existing worker diagnostic, rooting, NativeSafe and failure cleanup retained. No directory creation. |
| P36 | Init around existing `game.void`; init twice; new with `--source src/launcher.void` | First preserves source and omits entry; second preserves existing manifest and errors; new discovers launcher without required entry key. IDs are independently generated once. |
| P37 | Creation write collision, invalid ID, entropy failure, permission failure, interrupted/uncertain publication | Existing content unchanged; remove only provably owned fresh objects; report uncertainty; no automatic retry/prompt loop. |
| P38 | First new source: accept and complete review; decline; dismiss; close host while offered | One offer per root; accepted writes exclusive manifest; other cases create nothing. Reopen/new file/trust change does not repeat prompt. |
| P39 | Simultaneous first files, old tree opened, cancelled review, failed creation, explicit reset/Create Project | Fresh batch coalesces one offer; existing tree has no automatic offer; only explicit reset/command permits another offer/retry. Standalone analysis remains usable. |
| P40 | Untrusted/virtual/multiple-folder workspace; active file outside root; older project-incapable server | Existing workspace limitations/trust restrictions retained, no automatic compiler/write/load/install. Boundary/protocol errors visible; no silent fallback that loses manifest root. |
| P41 | CLI `--project DIR file.void`, `--entry` without project, profile/cache conflicts, literal forwarded `--app` | Selector/option conflicts are usage 2; forwarded literals preserve actual application-argument grammar. App deliberate status 1/2 is not relabelled compiler failure. |
| P42 | New project with corrupt/missing native receipts or mismatched installed stdlib/compiler resources | Preserve existing provider/resource rejection and reinstall diagnostics. Project metadata cannot supply replacement receipts, paths-as-authority or permissive linking. |

### Execution modes and native qualification gates

Run relevant runtime rows in unoptimized and O2 JIT, with eligible cold/warm Debug
cache variants, and in **independently linked** unoptimized/O2 AOT executables.
Capture command argv, actual SHA, toolchain/SDK/profile identity, status, stdout,
stderr and exact diagnostic spans. Assert absence of filesystem mutation/native
loading during manifest analysis and check. First-file tests include actual VS Code
host tests as well as finite state/command tests; handshake-only evidence is not UI
acceptance. Preserve existing low-stack/frontend, provider rejection, allocation,
GC, installed-tool, argument, alias, output publication and editor negative tests.

Keep the accepted seven-configuration shape in
[Compiler link experiment](../.github/workflows/ci-link-experiment.yml): Ubuntu 24.04
x64 Debug, Release and Sanitizers; Windows Server 2022 x64 Debug and Release;
macOS ARM64 Debug and Release. Preserve the selected link mode, LLVM/Clang/LLD
23.1.4, existing toolchain/provider pins, full unfiltered suites, editor steps and
artifacts. Do not alter that workflow or dispatch CI in this assignment. Preserve
fatal `ASAN_OPTIONS=detect_leaks=1:halt_on_error=1:exitcode=87` and
`UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1:exitcode=86`; no sanitizer recovery,
skip or negative-test weakening to obtain a green result.

Native Linux must show XDG/HOME placement parity, relocation/no-checkout AOT and
fatal-sanitizer/GC results. Native Windows must show wide/Unicode roots, exact-case/
reparse/device-name rejection, Known Folder behavior, existing SDK/CRT and output
publication rules, actual JIT/AOT and editor creation. Native macOS ARM64 must show
Foundation/bundle versus plain-process placement, ancestor policy, relocated AOT,
actual JIT/AOT and editor creation. Use existing native fixtures to control OS
failure/permission observations; do not claim one OS's errors prove another's.
Cross-compilation, Wine, a server handshake or one profile cannot qualify a missing
native platform/profile. Record missing evidence UNQUALIFIED. Prior accepted CI is
historical evidence, not acceptance of these unimplemented project changes. Boss
controls a future exact-SHA hosted gate and publication separately.

## Recommended implementation order and acceptance gates

These are minimal implementation slices under the continuing local authorization
and independent review boundaries above.
Keep Phase 1 application-project work separate from later native/package features.

| Slice | Deliverable and dependency | Acceptance gate |
| --- | --- | --- |
| S1 | Source-spanned schema/ID/SemVer validators and immutable ProjectConfig | Approve contract choices; P20–P24, exact positive examples/defaults, unknown/reserved rejection, no side effects. Select a bounded TOML parser with retained spans. |
| S2 | Separate root path/root scope; snapshot inventory/parser reuse and selection | S1; P01–P19, P24–P28, deterministic D/E split, library check scope, nested boundary and explicit compatibility. No new LLVM path. |
| S3 | CLI and ToolingSession/voidls project inputs/protocol | S2; P26–P28, P40–P41, disk/overlay parity, error-bearing sessions, legacy protocol unchanged and stale-result rejection. |
| S4 | IR metadata, JIT/AOT/bootstrap Runtime admission and four closed OS adapters | S1–S3; P29–P35, exact unchanged explicit-ID behavior, normal/O2 JIT and independent AOT, GC/cache/reload, no-source installed execution. |
| S5 | Exclusive init/new and VS Code first-file workflow | S1/S3, completed S4 for end-to-end app identity; P36–P40 including actual editor accept/decline/dismiss/concurrency and standalone recovery. |
| S6 | Integrated project qualification and canonical user documentation | S1–S5; all applicable P01–P42, relevant local suites, fatal sanitizers and honest native evidence. Stop for independent review. Hosted jobs wait for the full project/raylib gate above. |

S1 parsing helpers may be prepared independently of Runtime work only after scope
approval. Do not mask a missing library-analysis path, OS adapter or native platform
with an “MVP complete” label. Record per-slice implementation, local qualification,
independent review and hosted acceptance separately. Every implementation report
must name its actual baseline and changes; this design provides no executed result.

Later ecosystem work remains in the original dependency order: checked numeric
enums after grammar/result review; project resolver/locks and general typed C ABI;
dynamic raylib lifecycle integration with optional static qualification; debugging,
platform packaging, onboarding and public distribution. Retain verified native
artifacts/ABI probes, callback-generation/library lifetime, real window lifecycle,
missing/wrong-library diagnostics and installed no-checkout native acceptance as
those later gates. None is implemented in Phase 1; later local engineering follows
the continuing authorization, unresolved-decision checks and independent review
boundaries above.

## Unresolved design questions for independent review

The schema/algorithms above are a **complete recommended contract**, not a claim
that Boss approved these detailed choices. Approval is requested through Golden
Wrench's architecture review, not through an implementation action in this chat.

| Decision requiring Boss/Golden Wrench approval | Concrete recommendation to approve or revise |
| --- | --- |
| Initial scope and migration boundary | One target, library check-only; schema 1 fields/reserved-table rejection as specified; new scanning only with a selected manifest; all bare explicit-file/manifestless behavior retained. |
| Source eligibility and visibility | Exact roots/exclusion defaults/limits; D parse-only versus E semantic/startup graph; independently eligible imported app seeds remain candidates; library check has no application root-scope privileges. |
| Main plus legacy entry and script handling | Automatic search finds main only; selected source then retains existing legacy precedence; explicit project entry can select scripts or declarations-only sources. No signature/body-error fallback. |
| Metadata and binding surface | Embedded immutable record, once-only pre-global Runtime admission, code-6 missing-context result, finite direct-call OS adaptations and unchanged one-string function-value type. No general overload/default extension. |
| Creation UX and identity generation | Independent generated local IDs, init/new defaults/flags, explicit nested-project creation, persistent offered/accept/decline/dismiss suppression including failed/cancelled review. |
| Profiles, output and protocol | Release/O2 project default, explicit profile/cache conflicts, manifest-root build output, new separate preflight/project capability, legacy endpoint preservation and error-bearing project sessions. |
| Qualification and implementation release | S1–S6 boundaries and P01–P42/actual native seven-job gates. Implementation remains HOLD until separately authorized after review. |

Still deliberately unresolved **for later phases**, not hidden schema 1 blockers:
dependency constraint/lock grammar, multi-version/external import identities,
resolver versus VOIDUP command ownership, numeric enum syntax/result form and
`as?`/`??`, general C declaration/ABI/loading policy and static JIT, raylib revision,
platform debugger/package formats, production trust/distribution keys and release
clearance. Schema 1 reserves their space without accepting declarations as authority.

Keyboard stops after this documentation handoff for Golden Wrench's independent
review. No implementation, source tests, staging, commits, pushes, CI dispatch or
publication follows from recording this proposal.
