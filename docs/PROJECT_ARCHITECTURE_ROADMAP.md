# First-class VOID projects and native application roadmap

8 November 2026. Documentation and architecture planning only, prepared by
Keyboard Clanker for Golden Wrench's independent review. Inspected baseline:
`7a3e02008db64ee185aa4665aac5c76e3aff4b2b`. No implementation, new acceptance,
Git integration, CI dispatch or publication is authorized by this record.

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
| Selection | [source.cpp](../src/frontend/source.cpp), `discover_project`: explicit regular `.void` file wins; otherwise exact-case `main.void` search within the selected workspace, nearest ancestor first for an active file; labelled standalone editor fallback. [Discovery contract](STAGE8_DISCOVERY.md). | Replace selection in the compiler, with a versioned project-selection result shared by CLI and adapters. No editor scan for main. |
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

All schema below is **proposed design**, not accepted TOML grammar or a usable
configuration file. Prefer a small versioned TOML schema, bounded input, duplicate
and unknown-key diagnostics with spans, strict field types and no executable hooks,
environment interpolation or shell fragments. Validate without network access.

```toml
# Illustrative schema only. No entry key is required.
schema = 1

[project]
id = "org.example.orbit.sources"
name = "Orbit sources"
version = "1.2.3-beta.1+build.7"
kind = "application"             # alternative: library

[application]
id = "org.example.orbit"         # stable storage/application identity
name = "Orbit"
version = "1.2.3-beta.1+build.7"

[sources]
roots = ["src"]                 # default proposed root: manifest directory
exclude = ["src/experiments"]

[build.debug]
optimization = "none"
debug_info = "full"

[build.release]
optimization = "O2"
debug_info = "line"

[native.raylib]
version = ">=5.0.0 <6.0.0"        # example constraint, not a verified ABI claim
link = "dynamic"                # default; static requires explicit selection
binding = "raylib"              # binding identity, not loading authorization
```

For a library, omit `[application]`, set the library kind, and describe a separate
`[package]` with its own stable `id`, import `name` and `version`. A future project
containing app and library targets may carry both, with explicit target membership;
the first schema can restrict one build target. Never derive a package ID from an
application ID, project directory, display name or VFS path. A reusable package may
serve multiple apps and must not overwrite the consuming app's identity. A stable
project ID scopes persistent project identity; existing `/path.void` keys remain
within that scope. Moving the checkout preserves logical identity, not old physical
debug paths. Duplicated IDs across simultaneously loaded projects must diagnose.

Use full `major.minor.patch` semantic versions with optional prerelease and build
metadata. Parse numeric components and identifiers, reject leading-zero numeric
identifiers where disallowed, compare prerelease identifiers by semantic ordering,
and exclude build metadata from precedence. Application, project and package
versions have distinct ownership; propose an explicit project-version default
for omitted app/package versions. Native upstream version is separate from ABI
compatibility. Resolve constraints to exact versions/revisions and content hashes;
never use lexical version order, floating Git branches or build metadata as an
ordering tie-break. Constraint syntax and equal-precedence artifact policy need
review before schema freeze.

Proposed `[dependencies.<alias>]` entries describe local paths or Git repository
sources with stable package ID, version requirement and optional exact revision.
The lockfile records resolved commit/content identity, transitive edges, target
conditions, native artifact hashes and binding ABI identity. Resolve diamond/version
conflicts visibly; do not silently merge packages by display name. Keep project
`libs/` imports working; external packages require an isolated identity/visibility
domain before importing them can be enabled. Local dependency paths cannot escape
admitted roots merely because they appear in TOML. Multi-version imports, registry
support and lockfile name/format remain open.

Native entries add explicit per-target artifacts, architecture, OS, C ABI/calling
convention, CRT/runtime policy, ABI version and required symbols/layout metadata.
The example range does not prove compatibility. Reject unknown targets rather
than substituting another host's artifact. Model requested dependencies separately
from a verified, explicitly admitted link/load plan. No arbitrary linker command,
build script or package-provided executable runs during manifest validation.

Debug/Release select code optimization and debug information, not language safety
semantics. Retain checks, GC, overflow/conversion rules and the existing cache
eligibility policy in either profile. Proposed precedence: explicit command option,
selected profile, documented compiler default; report conflicts instead of guessing.
Also justify target selection, output directory and resource declarations, with
existing output collision/publication checks. Do not equate current `--no-opt`
with all Debug policy. Profile/target/dependency/metadata bytes participate in build
and cache identity. A profile must not silently change native ABI or trust.

An optional `entry` identifies an exact in-root source path, never a function name
or required filename. Its table placement is open. Missing/invalid explicit entries
diagnose with no automatic fallback. No generated template may make entry necessary.

## Proposed metadata delivery and OS locations

Validate the application ID using the existing [OS ID rules](os/API.md#application-placement),
including portable reserved-name rejection. Keep the ID stable across application
versions and display-name changes. Neither project nor package identity substitutes
for a missing application ID. Native bundle identifiers may need separately
validated platform mappings; they must not silently alter the storage ID.

Embed a versioned immutable metadata record in AOT application output; a private
compiler/Runtime interface publishes it before eager globals execute. A packaged
copy can aid inspection, but ordinary installed execution cannot depend on cwd,
the source checkout, a sidecar project manifest, environment guesses or online
lookups. The record contains app ID/name/version, project identity and relevant
build/target identity. Library metadata describes the library and never changes
the host application's context. Avoid embedding local source paths or credentials.

JIT passes the same validated record into the selected Runtime. Embedders supply
context explicitly; separate Runtimes can carry different apps. Reuse the existing
application-argument context pattern. Metadata changes require a fresh application
context/restart, not mutation under old globals, callbacks or generation leases.
Disk caches may retain immutable build metadata but never Runtime addresses, live
handles, roots or native loader state. Context identity must prevent stale cache use.

Propose `os.data_dir()`, `os.config_dir()`, `os.cache_dir()` and `os.state_dir()`
as zero-argument conveniences returning the same `result<string, os.error>` as
their explicit-ID forms. They obtain the consuming app ID from Runtime context and
then use the existing ID validation and platform placement kernel. **Preserve the
explicit-ID overloads**, their errors, owned strict UTF-8 paths, worker/effect
restrictions, NativeSafe/rooting and no-creation behavior. They discover locations;
they do not reserve storage, authenticate the app or grant `io` capabilities.

Proposed missing-context result: existing `unavailable`, with value-free operation
diagnostic and zero native code/domain. Invalid supplied IDs retain `invalid_argument`;
malformed manifest IDs fail project validation before execution. Do not guess from
`voidc`, package names or process-image filenames. Standalone applications can keep
explicit IDs. The exact overload mechanism requires review because the language
must not acquire general user-function overloading accidentally. This may use
compiler-owned facade binding, with both call forms covered by typed IR validation.
JIT image/resource/PID queries still describe the host as documented; application
storage identity is explicit context and must not change those observations.

## Proposed compiler-owned entry discovery

The approved filename rule supersedes older `main.void`-only **future direction**.
The old algorithm is still implemented until a separately authorized change lands.
`main.void` remains a valid ordinary filename and a historical example, with no
special selection priority in the proposed replacement.

1. Select one project boundary deterministically. Proposed manifest lookup walks
   the active directory's ancestors only inside the explicitly selected workspace;
   the nearest exact `void.project.toml` wins. CLI directory selection examines
   that selected root. Stop at nested project boundaries. Reject malformed manifests,
   alias/case collisions and invalid paths instead of searching for a different app.
   An explicit project selection takes precedence; no unbounded home/cwd search.
2. Build a bounded source inventory through compiler-owned loader admission. Use
   declared application source roots/exclusions; proposed manifestless directory
   default is the selected root. Traverse `.void` sources deterministically,
   excluding build outputs, `libs/`, external dependencies, test/example directories
   and nested projects from automatic application seeds. Freeze exact default
   exclusions and resource/work limits before implementation. Paths still obey
   containment, exact-case, portable naming, symlink/reparse and hard-link rules.
   Exclusions never permit bypassing safety checks on an actual import.
3. Parse those application seeds with the existing frontend and expand their import
   closure through `SourceGraph`, retaining immutable disk/overlay snapshots. A seed
   is eligible because it belongs to the selected application source set; import
   reachability alone never makes a library, stdlib or dependency entry-eligible.
   Sort diagnostics/candidates by canonical identity and span, independent of
   filesystem enumeration order. Do not select via regex, text search or an LSP parser.
4. Identify top-level synchronous zero-argument `main()` declarations with existing
   allowed void/i32 results. Nested functions, methods and dependency main helpers
   are not candidates. A malformed entry-shaped main produces its source diagnostic;
   do not hide it by choosing another file. Exactly one valid candidate is selected.
   More than one is an ambiguity error listing all locations and an explicit-entry
   remedy, even if one lives in `main.void`. No candidate is an executable-target
   diagnostic; library/check targets do not require main.
5. Before execution, validate the selected source graph with the same typed entry
   classification, global initialization, import/effect and IR gates used today.
   Proposed first version validates all application seeds, making errors in the
   selected application source set visible even when not imported by main. Root
   constants/globals still belong to the selected entry module, not every seed or
   the manifest. Enumeration cannot execute initializers or load native code.

Explicit CLI source paths and the editor entry override continue to select the
requested file with existing main/top-level/legacy classification and precedence.
Propose applying a manifest root only when a project context is explicitly selected
or already selected by the editor; bare explicit-file CLI usage keeps its parent
root for compatibility. In project context the override is in-root and does not
move the manifest's VFS boundary. A future project option's spelling needs review.
CLI and editor must produce the same result for identical context and snapshots.

Automatic discovery searches for main, not arbitrary executable top-level files or
legacy functions. Standalone/explicit scripts retain executable top-level order,
zero fallthrough, existing legacy numeric input/result behavior, and declarations-only
execution. In a project, a script can use an optional explicit entry. Imported
executable statements remain errors; ordinary libraries remain valid without main.
The broader future seed inventory is an intentional change from today's strictly
entry-reachable graph, not a claim that the old resolver already scans sources.

Manifest/source-set/entry changes create a new compiler selection revision.
Tooling may show a pending reselection but must not silently rebase dirty overlays,
publish stale diagnostics or hot-reload changed project identity. Preserve the
existing explicit restart/reselection and snapshot-version guards.

## Proposed first-file creation and terminal workflows

In a trusted supported filesystem workspace, observe creation or first save of a
new `.void` file. Ask the compiler project preflight whether an enclosing manifest
exists within the selected boundary and whether this is the first eligible source
creation in that root, using bounded compiler-owned inventory rather than a second
extension source resolver. Opening an existing file, installed stdlib,
virtual/untitled buffer or dependency must not trigger project creation. Do not
retroactively classify an existing source tree as a first-file event on activation.

Offer **Create VOID project** or **Keep standalone** once per workspace/root
identity. Persist accepted/declined/dismissed state in extension workspace storage,
not a source-controlled preference file; coalesce concurrent file events. Treat
dismissal as suppressed for that root until a deliberate Create Project command
or reset. After accepting, preview/select the destination and metadata; use an
exclusive create, preserve the source and never replace an existing manifest.
Recheck after races. Failed creation remains retryable through the command and
must not produce a prompt loop. Untrusted workspaces retain existing syntax-only
behavior; no compiler spawn, filesystem write, dependency install or native load
follows merely from a file event. Other editors can invoke the terminal workflow.

Proposed `voidc init [directory]`: create a minimal validated manifest around an
existing source tree, preserve every existing source filename, offer app/library
kind and stable identities, and omit entry by default. Proposed `voidc new <path>`:
exclusively create a new project directory, manifest and small starter source with
any chosen valid filename. A template filename is convenience, never a compiler
requirement. Both support explicit noninteractive metadata options, give actionable
collision/identity errors and perform no dependency installation or native execution.
Command flags, interactive defaults, rollback of only newly owned files and how to
generate a stable project ID remain review questions. `init` must not rename
`game.void` or insert a mandatory `entry = "main.void"`.

## Proposed numeric-backed enums and fallible conversion

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

## Recommended implementation order and acceptance gates

Every row below is future work, with native execution recorded per platform and
unavailable evidence labelled UNQUALIFIED. No new test or CI result is claimed.

| Order | Deliverable | Acceptance gate before dependent work |
| --- | --- | --- |
| 1 | Review schema, identity boundaries, source graph and protocol design | Freeze minimal schema/defaults, SemVer/constraints, manifestless compatibility, exclusions/limits, native admission ownership and unresolved questions below. Golden Wrench reviews this architecture; Boss controls implementation scope. |
| 2 | Compiler manifest parsing and shared selection/discovery | `app.void`, `program.void`, `game.void`, nested valid source paths; zero/one/multiple candidates; a main.void plus another main must be ambiguous. Test explicit override, wrong signatures, imported main, libraries, top-level/legacy behavior, malformed/aliased paths, nested projects, deterministic ordering, unsaved overlays and dirty-session reselection. No execution/network during check. |
| 3 | Immutable metadata and project-aware OS calls | Normal/O2 JIT and independent AOT agree; installed app runs after checkout/manifest removal. Test eager globals, absent/invalid context, two Runtime identities, unchanged explicit-ID errors/placement/security, GC, reload and stale-cache rejection. |
| 4 | `init`/`new`, VS Code creation and project tasks | Accept, decline, dismiss, reopen, concurrent first files, existing manifest race, failed write, untrusted workspace and standalone operation. No repeated prompts or clobbering; arbitrary source names survive init; CLI/editor selection and diagnostic parity. |
| 5 | Numeric-backed enums and checked conversion | Freeze syntax/result shape first. Check all integer widths/signedness, backing bounds, gaps, negative values, duplicates policy, lazy fallback, constants, malformed syntax and payload rejection; independent IR rejection and normal/O2 JIT/AOT parity, GC/reload compatibility. Can proceed independently after language-design review. |
| 6 | Resolver, locks and generic typed C ABI foundation | Reproducible/offline resolution, visible conflicts, exact target/ABI/hash admission, no install hooks, correct C layout/call ABI probes and negative libraries. Separate package trust from metadata and editor analysis. Preserve current trusted provider tests. |
| 7 | Dynamic raylib vertical slice; optional static path | Real native lifecycle and resource cleanup, native dependency diagnostics, callback shutdown/reload/library leases, installed relocated AOT with source/compiler absent, JIT/AOT parity; static independently qualified or explicitly unsupported per mode. Native Windows/macOS/Linux evidence, not cross-build inference. |
| 8 | Editor/debug, packaging, onboarding and public release preparation | Fresh-user install/init/raylib/run/package walkthrough without hand linker commands; native loader/debugger tests, versioned docs/examples, dependency/source/license inventory, distribution trust and platform package review. Boss separately controls Git, hosted acceptance and public release. |

## Unresolved design questions for independent review

- Minimal one-target schema versus app/library multi-target projects; table/entry
  placement; project ID generation and clone/collision handling; metadata inspection API.
- Exact source-set defaults/exclusions, bounded recursive inventory policy, nested
  project selection and explicit-file/project-context option. Confirm validation of
  all application seeds and entry-root constant/global visibility under a manifest root.
- Semantic version constraint grammar, equal-precedence artifacts, lockfile format,
  Git/local trust admission, multiple package versions and external import identity.
- Zero-argument OS facade binding without general overloading; missing-context
  `unavailable` recommendation and native bundle/storage ID mapping.
- Numeric enum declaration/backing/alias rules, nullable or result conversion shape,
  `as?`/`??` grammar and exact diagnostics; keep payload variants distinct.
- Binding syntax/generation, supported initial C subset, per-target ABI probes,
  native dependency trust/loader guarantees, static JIT scope and callback provenance.
- Resolver command owner and VOIDUP integration boundary; production trust keys,
  platform debugger/package formats and distribution qualification remain separate.

Review ends with this documentation handoff. Keyboard must not begin implementation
or Git/hosted work as a consequence of recording these decisions.
