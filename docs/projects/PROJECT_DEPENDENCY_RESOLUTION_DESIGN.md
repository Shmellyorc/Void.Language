# VOID project dependency resolution

Design for Golden Wrench review at `9902abee531332bc56bec6fbf451fa23a6e5bb38`.
Start with an offline compiler-owned resolver for local library projects. Add
explicit acquisition of Git commits and native payloads in separate slices.

## Integrated foundation

| Area | Implemented at this baseline |
| --- | --- |
| S1 manifest | `void.project.toml` schema 1 is a bounded, pure TOML parse with owned source spans, separate project/app/package identity roles and exact SemVer values. The parser rejects nonempty `[dependencies]`. S7 added local `[native.linux]` and `[native.windows]` descriptor, provider and adapter filenames. [Manifest model](../../include/void/project.hpp) · [Parser](../../src/frontend/project.cpp) · [SemVer implementation](../../src/project_identity.cpp) |
| S2–S3 discovery and tooling | Manifest selection, nested-project boundaries, automatic application entry selection, explicit entry overrides, library checking, CLI profiles and project-aware tooling sessions are integrated. `ProjectCapture` separates discovery from the execution graph, owns observations and recaptures source bytes before publishing a result. `SourceGraph` resolves project VFS paths, local `lib/` imports and the separate standard-library domain. Standalone explicit-file commands retain legacy semantics. [Project selection](../../src/frontend/project_selection.inc) · [SourceGraph](../../src/frontend/compiler.cpp#L5284) · [Tooling](../../include/void/tooling.hpp) · [Local imports](../LOCAL_LIBRARIES.md#source-spelling-and-layout) |
| S4–S6 app and language | Immutable application metadata reaches the Runtime and app-scoped OS APIs. `voidc init/new` and the VS Code first-file workflow exist. Numeric-backed enums and checked conversions are integrated. Library projects are checked as source and have no application entry or app metadata. [Creation](../../src/frontend/project_creation.cpp) · [Runtime application context](../../src/runtime/application.cpp) · [CLI](../../src/main.cpp) · [Integrated source changes](https://github.com/Shmellyorc/Void.Language/commit/a642ce746a62a7646d06d42d9f87931813aaa812) |
| S7 C FFI | Checked descriptors and generated adapters use the same typed IR and LLVM JIT/AOT. G1–G5 cover scalars, records/spans, owned resources, dynamic admission and scoped/retained callbacks. Conventional loading is qualified on Linux x64 GNU and Windows x64 MSVC. It can execute native initializers before symbol or adapter checks. The separate Linux G4-L profile verifies a bounded `system none` closure. Raylib 6.0 exercises the general facility. Runtime ownership, precise GC roots, thread affinity, callback drain and generation/library leases are part of the contract. [Closeout](SLICE7_FINAL_CLOSEOUT.md) · [Conventional loader](SLICE7_G4_CONVENTIONAL_FFI.md#loader-and-lifetime) · [Verified Linux profile](SLICE7_G4_LINUX.md) |
| Build identity | The CLI's JIT key includes the project selection/profile and compiler build identity. The backend also keys on the target, ABI, compatibility data and module identities, paths, imports and bytes. C primitive admission bypasses persistent JIT reuse, and the Windows persistent cache has no publication path. AOT uses the compiler's verified link resource inventory. [CLI](../../src/main.cpp) · [LLVM backend](../../src/backend/llvm/backend.cpp#L2504) · [Object cache](../../src/backend/llvm/native_cache.cpp) · [Native build](../../src/native_build.cpp) |
| VOIDUP | The toolchain installer has signed catalog authentication and freshness checks, bounded same-origin transport, size/hash checks, inventory-checked extraction, private directory capabilities, writer locking and staged publication. Reuse applicable primitives and verification patterns. Keep the toolchain catalog, signing keys, activation state, executable dispatch and install store in VOIDUP. [Trust](../../apps/voidup/trust.void) · [Transport](../../apps/voidup/transport.void) · [Inventory](../../apps/voidup/inventory.void) · [Install flow](../../apps/voidup/manager.void) |

The roadmap, development strategy and local/native library guides contain
dated proposal and status sections. The integrated source and final closeout
establish the baseline here. The closeout records full Linux Release, Debug and
fatal sanitizer runs and a selected Windows Release/Debug matrix. macOS native
FFI remains unqualified. The remaining gaps are dependency declarations and
graph resolution, external package visibility/identity in the compiler, source
acquisition and verified snapshots, lock generation and consumption, managed
native artifact selection and staging, and an end-user packaging workflow.
[Roadmap](../PROJECT_ARCHITECTURE_ROADMAP.md#proposed-dependency-ownership-and-voidup-integration) · [Strategy](../DEVELOPMENT_STRATEGY.md) · [Native library history](../NATIVE_LIBRARIES.md) · [Closeout](SLICE7_FINAL_CLOSEOUT.md#qualification-scope-and-preserved-platform-boundaries)

## Source declarations and resolution

Extend schema 1's reserved dependency table with a closed set of fields.
Existing compilers reject populated tables. These are proposed manifest excerpts,
with the Git form enabled after the local slice.

```toml
[dependencies.geometry]
id = "org.example.geometry"
path = "deps/geometry"
version = ">=1.4.0, <2.0.0"

[dependencies.color]
id = "org.example.color"
git = "https://example.org/void/color.git"
rev = "0123456789abcdef0123456789abcdef01234567"
version = "=2.1.0"
```

- The key is a consumer-local import alias using the existing package identifier
  grammar. `id` matches the dependency's `package.id`. The library's effective
  `package.version` uses `project.version` as its schema 1 default. Project ID
  still identifies a project, app ID still controls app metadata/storage, and
  `package.name` does not set the consumer's alias. Only `kind = "library"`
  satisfies a package edge. Require `package.entry` for libraries consumed as
  dependencies. It names a package-relative `.void` module whose public
  declarations form the import surface. Standalone library checks can omit it.
- Use exactly one locator. A path is relative to the declaring project, within
  the selected root project tree for local sources or the pinned repository
  tree for Git sources. Permit parent segments for sibling packages only while
  every step stays in that enclosure and passes exact-case and alias checks.
  Exclude administrative and build directories from locator traversal. Lock
  the normalized enclosure-relative location. Git uses an explicit HTTPS
  origin and full commit object ID, with optional portable `subdir` defaulting to
  the repository root. Start with full lowercase SHA-1 Git IDs as locators and
  independent SHA-256 content identity. Reject credentials in URLs, mutable
  refs, URL rewriting and fallback to other origins.
- V1 accepts `=<full SemVer>` for exact text identity or a comma-separated
  intersection of `>`, `>=`, `<` and `<=` comparisons over full stable
  versions. Reuse the existing comparator. Range endpoints omit build metadata,
  ranges ignore candidate build metadata for precedence, and prereleases require
  an exact pin. Reject empty, contradictory and unsupported requirements.
  A path or full commit supplies one candidate, so ranges check compatibility
  of that candidate. Version discovery across tags or a catalog is a later
  decision with its own selection policy.
- Traverse all declared edges, including currently unused ones, from the selected
  root in bytewise alias order. Resolve each locator once and validate every
  incoming requirement. Within a graph, one package ID selects one origin,
  exact version and content hash. A diamond shares that selection. Different
  locators for one ID conflict even when the bytes match. Report both dependency
  chains and source spans on version or origin conflicts. Detect cycles with a
  visiting stack and report the closing chain. Existing declaration cycles
  within a project or package still follow source validation rules. Bound graph,
  path and input work and fail the operation on exhaustion.
- Proposed `use pkg/geometry as geo` selects the direct edge's public entry.
  Within that package, quoted imports and `lib/` resolve against its own root
  and `pkg/` against its own direct edges. A package can call its dependency's
  public API. Each package's `internal` declarations stay in its own domain.
  Preserve existing file and lexical ownership rules for `private`. Reject
  quoted access to reserved package identities and traversal to another
  package's root. Package code never enters application entry discovery.
  Imported modules use the existing declaration and top-level execution rules.

## Lock, acquisition and compiler integration

**Lock contract.** The selected root owns `void.lock`, including when a library
is checked on its own. Consuming projects follow each library's manifest
requirements. The lock is deterministic TOML with a format revision, resolver
semantics revision, root project ID and kind, and SHA-256 of canonical effective
dependency inputs. The root input digest covers alias/locator/constraint mappings
and managed native requirements, independent of comments and declaration order.
Record each selected package ID, full version, canonical locator and applicable
commit, manifest and source-tree digests, and alias-labelled outgoing edges, including
edges from the root. Lock native selections separately by target. Canonical
UTF-8/LF serialization uses fixed fields and bytewise ordering, with relative
portable paths, no timestamps, checkout locations or host cache paths. Commit
the lock with the manifest. A lock records expected bytes. Publisher trust
comes from a reviewed pin or approved provenance policy outside the lock.

**Content contract.** Define tree digest v1 as SHA-256 of a domain tag and a
bytewise path-ordered sequence of framed records. Frame each entry kind and
relative path, and regular-file contents, with unsigned 64-bit big-endian byte
lengths. Hash regular-file bytes verbatim, including the manifest. Exclude
top-level `.git`, `.void`, `build` and `void.lock`. Record a nested project
boundary as a path marker and hash its files under its own package if selected.
Treat empty directories and mode/timestamp differences as irrelevant. Reject
nonportable names, case collisions, links, reparse points, submodules and special
files in the exported input. The compiler reads only this admitted inventory and
must enforce its boundaries even on quoted paths. Git exports commit blobs with
their original bytes and without working-tree filters, submodule checkout or LFS
smudge. Local path projects use the same inventory rules and must still match at
locked consumption, so edits to dependency inputs require an explicit refresh.
Start with caps of 256 packages, 1,024 edges, 64 graph levels, 8,192 files and
64 MiB per package, 256 MiB total source and 1 MiB per lock. Apply traversal and
diagnostic caps before hashing or parsing. [Git tree enumeration](https://git-scm.com/docs/git-ls-tree) · [Git checkout filters](https://git-scm.com/docs/gitattributes)

**Operations.** Put pure graph and lock validation in the compiler's project
layer. `voidc resolve --project .` consumes local trees and already acquired
snapshots, shows exact selections and writes a complete lock. It can validate an
existing lock idempotently. Require `--refresh` to replace a differing lock.
Validate against the observed root and input snapshots, then publish by checked
atomic replacement with a writer lock. Leave the previous lock usable after a
failure. `check/run/build/emit` and project tooling consume dependencies locked
and offline by default. Verify required lock structure, root fingerprint, edge
completeness/reachability and constraints against verified manifests, and bytes
against pins before using them. Missing, stale, unsupported and tampered inputs
have distinct diagnostics with recovery commands. There is no automatic
resolution or dependency-store repair during analysis or builds. Projects with
no dependency declarations and no lock need no new setup.

Only an explicit acquisition command, proposed as `voidc deps fetch`, accepts
an origin plus commit or an approved set of exact pins from a reviewed lock.
A newly encountered transitive origin requires its own authorization. Start
with local Git fixture imports and an approved HTTPS transport adapter later.
If using installed Git, use an explicitly selected trusted executable, literal
arguments, a private object store, a scrubbed environment and config, and disable
hooks, helpers, filters, alternate protocols and redirects outside the admitted
origin. Export data into private staging, verify the expected snapshot when
rehydrating, then publish immutably under its digest. Index the admitted origin
and commit separately so a cache hit cannot substitute bytes from another pin.
Use held input bytes for source compilation and verified staged files for
native handoff. Interrupted or concurrent writers must never expose incomplete
entries. Reuse bounded hashing
and held-directory/publication capabilities where their platform and ownership
contracts match. No package-defined subprocesses, configure/build scripts or
implicit native authority. [Host crypto](../../src/runtime/host_crypto.cpp) · [POSIX directory capabilities](../../src/runtime/host_directory.cpp) · [Windows directory capabilities](../../src/runtime/host_directory_windows.cpp)

**Compiler seam.** Admit the read-only resolved plan after manifest selection
and before `ProjectCapture` loads the execution graph. Extend `SourceGraph`
with per-unit ownership, direct-edge resolution and package-relative readers
under a reserved identity such as `/@pkg/<package-id>/...`. Key semantic
ownership on the selected package ID within the graph and keep version, origin
and digests in snapshot/provenance keys, separate from physical diagnostic paths.
Update frontend `same_library` and IR access checks together. Today several
checks distinguish only project and stdlib. Enforce package ownership for
`internal` access and scope conveniences such as root constants to
the owning project or package, and validate ownership and import edges in IR.
Resolve package imports, tooling navigation and completion
through the same graph. Existing root overlays stay root-scoped. Revalidate the
lock and captured inputs with project observations before publishing a snapshot.
Include the effective plan digest in project selection, JIT/cache provenance and
AOT build inputs. A changed package/native selection forces a fresh analysis
and a restart for live code; existing Runtime generations keep their leases.
Keep the existing cache bypass for C primitive admission.
[Frontend visibility](../../src/frontend/compiler.cpp#L657) · [IR visibility](../../src/ir/interfaces.inc) · [Capture and recapture](../../src/frontend/project_selection.inc) · [AOT build](../../src/native_build.cpp)

## Managed native artifacts

Add a separate application-root declaration when the native slice starts, for
example `[native-requirements.raylib]` with
`id = "org.example.raylib"`, `version = "=6.0.0"` and `abi = 1`.
Default to dynamic linkage. Treat existing local `[native.linux]` and
`[native.windows]` as the conventional manual workflow. Reject mixing a managed
provider with a local provider in the initial lane. Support one managed
provider/adapter pair per application target, matching the current single
`CPrimitiveAdmission` and Runtime binding. Initially only the application
can supply FFI binding authority. Defer package-authored native requirements
and multi-provider composition until the ABI, namespace and lease model has
independent review.

Select from an explicitly approved local artifact catalog first. Lock each
selected target's provider ID, upstream library version, C ABI revision,
descriptor and adapter identity, provider build revision, linkage, target
triple and relevant CRT/libc or minimum OS. Record exact filenames, lengths and
SHA-256 for the descriptor, adapter, provider and bundled transitive libraries,
with their loader names, system import policy and notices. Record link inputs
such as an import library separately if the recipe uses one. Package SemVer
alone does not establish ABI compatibility. Compare descriptor ABI/dispatch and
target layout/calling convention, provider revision, image architecture and
the supplied payload before admitting it. A lock may list several explicit
target lanes; a missing lane fails with no host fallback. Keep toolchain
selection and resource verification separate from these package artifacts.

Use `CPrimitiveAdmission` to supply the existing descriptor and typed IR bridge.
Check and editor analysis can inspect metadata without loading a library.
Run and native code generation retain explicit `--allow-native` with an
approved plan. Build stages verified files for the existing executable-relative
`native/` layout without running provider initializers. Embed the selected
artifact contract in the AOT executable so a relocated launch can verify the
payload without a checkout, lockfile or installed compiler. JIT admission
and AOT startup verify the declared payload before loading. Bind
ownership and release through the existing Runtime and generation leases.
For conventional ELF/PE loading, verification covers the declared files.
Actual transitive binding still follows OS loader rules, including initialized
or already-loaded modules. Any stronger claim requires a separately qualified
loader policy. Preserve the existing Linux G4-L guarantees as a separate lane.
Static archives are an optional later AOT lane with explicit archive order,
closure, PIC/CRT and redistribution requirements. Static JIT requires its own
qualification. [Admission](../../src/ir/c_primitive.cpp) · [Runtime loader](../../src/runtime/c_dynamic.cpp) · [G4-L contract](SLICE7_G4_LINUX.md)

## Practices that fit VOID

- Cargo separates requirements from exact selections in `Cargo.lock` and
  distinguishes locked from offline operation. Its `links` constraint is a
  useful precedent for a single native provider identity. VOID can make locked
  offline consumption the default and limit acquisition to explicit commands
  and package data. [Cargo resolver](https://doc.rust-lang.org/cargo/reference/resolver.html) · [Locked/offline flags](https://doc.rust-lang.org/cargo/commands/cargo-vendor.html) · [Native links](https://doc.rust-lang.org/cargo/reference/build-scripts.html#the-links-manifest-key)
- Go's MVS computes a build list from minimum requirements, while `go.sum`
  authenticates module content hashes. The separation between version choice
  and content verification fits VOID. Pinned locators and upper bounds give
  VOID a different conflict rule, so use a graph walk over the fixed candidates.
  [Go version selection](https://go.dev/ref/mod#minimal-version-selection) · [Go checksums](https://go.dev/ref/mod#go-sum-files) · [SemVer precedence](https://semver.org/spec/v2.0.0.html)
- CMake's ExternalProject supports pinned Git commits and archive hashes.
  Imported targets distinguish runtime DLLs from link-time import libraries.
  Apply those ideas to native inventory roles and preverified payloads. Keep
  CMake in controlled provider preparation, outside project dependency
  consumption. [CMake ExternalProject](https://cmake.org/cmake/help/latest/module/ExternalProject.html) · [Imported libraries](https://cmake.org/cmake/help/latest/command/add_library.html#imported-libraries)

## Implementation and qualification

| Slice | Acceptance |
| --- | --- |
| D1 local resolution and lock | Extend the manifest, add a pure fixed-candidate resolver and local snapshot reader, and implement resolve/refresh and lock verification. A direct edge and diamond yield byte-identical locks after relocating the same inputs, shuffling enumeration or reordering root dependency declarations. Reject wrong ID/kind, malformed SemVer, disjoint constraints, different origins or revisions for one ID, cycles, escapes, aliases, exhausted budgets, stale or malformed locks, and missing/extra edges. Prove changed source bytes fail verification. Inject interruption and competing writers and verify the previous lock or a complete new lock remains. |
| D2 compile the locked graph | Add package entry/import domains and read-only plan consumption to CLI/tooling. Run a diamond through check, Debug/O2 JIT and linked AOT with matching output and resolved graph. Test same module names in different packages, internal/private access rejection in frontend and IR, undeclared transitive imports, package main isolation, cache invalidation and editor navigation. Verify no network, acquisition process, native initializer or dependency state writes from check/LSP. Keep existing local-library, explicit-file and project profiles working. |
| D3 explicit Git acquisition | Use disposable local Git repositories before HTTPS. Compare locked commit blob export against the expected digest; test wrong commit and hash, moved refs, malicious names, link/submodule entries, interrupted fetch, and unauthorized transitive origin. With transport unavailable, a populated store builds using the committed lock and an empty/corrupt store fails with the exact missing or corrupt pin. Exercise hostile Git config and origin changes before enabling HTTPS. |
| D4 one managed dynamic provider | Resolve a local, prebuilt provider/adapter payload for each supported lane. Exercise explicit authorization, headless real calls, callback/owned-resource teardown, normal/O2 JIT and linked AOT relocation with checkout and compiler absent. Test missing target, wrong architecture, ABI or provider revision, substituted payload, missing transitive file, and unauthorized launch/link paths. Instrument an initializer to distinguish rejection before load from checks made by the loader afterward. Reuse the existing C FFI fixtures and qualify raylib through the same plan. |
| D5 packaging and optional static | Package the executable, verified runtime payload and notices with no dependency on the project/compiler install paths. Qualify loader behavior per target before distribution. Review static AOT and byte-identical binaries as separate gates. |

**Reproducibility target.** Identical source snapshots and resolver inputs give
the same lock and target selection across checkouts. The same target, compiler
build identity, verified stdlib/Runtime/link resources, profile and source
snapshots give the same dependency inputs and test behavior. Object and binary
byte equality needs a separate build-path, debug-info and linker determinism
gate. Keep normal compiler outputs and caches separate from the lock contract.

**Native platform gates.** Each OS independently runs D1–D3 for source resolution,
offline consumption, filesystem/Unicode paths, concurrency and relocation.
Linux x64 GNU qualifies ELF architecture, SONAME/RUNPATH, declared closure and
conventional versus G4-L behavior, including fatal ASan/UBSan with leak detection
for headless tests. Windows x64 MSVC qualifies PE architecture, DLL search and
same-name reuse behavior, import libraries versus static archives, CRT and
native ACL/reparse handling. macOS qualifies the selected architecture on a
native host, including Mach-O slices, minimum OS, install names, `@rpath`,
signing policy and a relocated launch. Keep managed macOS native execution
unsupported until the loader integration and its native tests pass. Linux or
cross-compiled results provide no Windows or macOS execution acceptance.

Replay using the pinned LLVM/Clang/LLD 23.1.4 build and matching provider
inputs. Use native Release/Debug and applicable strict profiles. Extend the
existing project, module, cache and FFI suites. Keep build trees and fixtures'
generated outputs in task-owned disposable workspaces.

```sh
cmake --build "$build" --parallel 2
ctest --test-dir "$build" --output-on-failure \
  -R '^(project_|modules($|_)|local_libraries$|debug_cache|ffi_g[1-5]_|runtime_callbacks$|cli_install)'
```

Keep maintained source fixtures and concise replay instructions. Preserve the
eight retained historical roots listed in the policy. [Configure and install](../SOURCE_COMPILER.md#building-the-compiler) · [Retention policy](VOID_ARCHIVE_AND_RETENTION_POLICY.md#phase-2-local-cleanup)

## Decisions for Golden Wrench

1. Approve the schema 1 extension, `pkg/` plus a single explicit library entry,
   package-domain visibility, and one pinned source per package ID. Decide
   whether selectable version catalogs or side-by-side versions merit a future
   resolver revision.
2. Confirm `voidc` owns the resolver and lock, and approve the separate
   acquisition command and trust scope for an origin/commit. Review relative
   path enclosure, export exclusions and digest framing before freezing v1.
3. Approve one root-managed provider per target and define how the local
   artifact catalog is trusted. Confirm the conventional loader guarantee,
   application authorization, the macOS target/signing scope, and what must
   qualify before packages can declare native requirements themselves.
4. Approve locked/offline defaults and input-level reproducibility as the
   first gate. Authorize D1 separately after design review.

This pass inspected the integrated source and researched the design. Existing
qualification results are cited from their closeouts. The proposed slices have
no implementation or test results yet.
