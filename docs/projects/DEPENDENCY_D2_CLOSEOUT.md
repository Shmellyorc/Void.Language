# D2 locked package compilation closeout

Implementation against `a5251505881fb8f6aaf5028aa735cc36cfb59583`, the reviewed D1
integration. A manifest application can lock a local dependency with
`voidc resolve --project DIR`, import it with `use pkg/geometry as geo`, and
run or build through the existing compiler. A library selected as the root
checks its imports against its own lock. A consumed library uses the verified
graph of the consuming project.
[D1 closeout](DEPENDENCY_D1_CLOSEOUT.md) · [Dependency design](PROJECT_DEPENDENCY_RESOLUTION_DESIGN.md)

## Compilation contract

- `pkg/<alias>` uses only the importing project's declared direct dependency
  table and opens the selected library's `package.entry`. Each consumed library
  uses its own direct edges. The importer can supply an explicit source alias.
  The default qualifier is the manifest alias.
- Root project files, each locked package, and standard-library files have
  separate source ownership. A locked source has the identity
  `/@pkg/<package.id>/<package-relative-file>`. Physical paths provide diagnostics
  and navigation. The package ID and relative filename survive relocation.
  Quoted paths and `lib/` inside a package resolve within that package. Package
  sources remain outside application entry discovery.
- The D1 resolver recomputes the canonical lock against the effective project
  manifest and captures the exact package source bytes it hashes. The existing
  SourceGraph parses those bytes. Project capture verifies the package inputs
  again before it accepts the analyzed snapshot. Missing, malformed, stale or
  tampered locks fail before compiler execution or cache lookup. Compilation
  never publishes the lock. Explicit resolution retains the existing D1 writer
  and lock format. Dependency-free projects still compile without a lock.
- Public declarations use existing import visibility. Private declarations keep
  their file and lexical rules. Internal declarations require a shared ownership
  domain. Package code receives no implicit access to the application entry
  scope. Typed IR independently checks admitted source ownership, direct
  package-entry edges and callable visibility. Package imports grant no native
  provider admission.
- The SHA-256 of the verified lock contributes to project selection and the
  compiler compatibility contract. Existing cache, native generation and AOT
  provenance consume that contract. Refreshing after a package change requires
  a new Runtime generation. A rejected reload leaves the previous generation
  live.
- The existing ToolingSession and `voidls` use the same source identities. Package
  import completion uses direct aliases. Definitions open the physical admitted
  files, and package diagnostics retain their source URI and UTF-16 range.
  Package files are read-only in the consuming session. Edits to package inputs
  on disk require explicit lock refresh and editor reanalysis. Checked library
  projects retain the existing semantic-only interface.

## Qualification

All Linux builds used LLVM/Clang/LLD 23.1.4 and the verified host and
AWS-LC 5.12.0 HTTP provider prefixes. The two selectors below cover 15 distinct
registrations. Both repeat the package fixture and the project creation
process. The second also exercises the cache, Runtime globals, conventional
FFI and FFI callbacks.

| Platform and compiler profile | Result |
| --- | --- |
| Linux x86-64 Release | 15/15 distinct tests, with 11/11 focused and 6/6 regression |
| Linux x86-64 Debug | 15/15 distinct tests, with 11/11 focused and 6/6 regression |
| Linux x86-64 RelWithDebInfo with fatal ASan/UBSan and leak detection | 15/15 distinct tests, with 11/11 focused and 6/6 regression |
| Native Windows Release and Debug | UNQUALIFIED |
| Native macOS ARM64 Release and Debug | UNQUALIFIED for D2 |

An early Release probe used a compiler object that had started compiling before
the editor changes landed and failed package completion. The rebuilt compiler
passed the real-server fixture in Release. An early strict pass also picked up
an IR probe compiled before the fixture added a dependent global. Qualification
uses the rebuilt probe.

The maintained `project_packages` registration uses a private process fixture.
It exercises application and library project checks, a direct and transitive
diamond, same-named sources, package-owned quoted and `lib/` imports, and a
standard-library call. Negative tests cover undeclared edges, internal and
private access, application-root authority, nested boundaries, reserved
identities, escapes and forged typed-IR edges and calls. The fixture compares
normal and optimized JIT with separately linked executables, emits an object,
checks dependency initialization order and shared package globals across a
collection, verifies their handles are released on Runtime shutdown, checks
cache hit and miss, and rejects an
incompatible live reload. It also checks relocation, unchanged lock bytes and
mtime, lock failure across compiler commands, real stdio completion and
navigation, package diagnostic recovery and dependency-free local libraries.

The final integration review rebuilt Release in a disposable workspace and
reran `project_packages` and the three D1 registrations. All four passed.
A private probe changed only a package README and verified a cache miss and
different AOT object bytes after lock refresh. Repeated emits before the
change matched. A separate package source edit was rejected before refresh
and ran through a freshly linked executable afterward. Both project checking
and lock refresh rejected a package symlink pointing outside the project and
a library manifest that tried to declare a native provider.

The running VirtualBox guest named `Windows 11` identified itself as Windows
10 `10.0.19045.6466`. The existing Guest Control login failed with
`VBOX_E_IPRT_ERROR (0x80bb0005)` and “The specified user was not able to logon on
guest.” Native execution remains unavailable through that management path.
The prior D1 Debug AOT signal 11 remains an unresolved intermittent observation.
It did not recur in the D2 Debug runs of the registered creation process,
including the run that overlapped the cache process test.
[D1 crash investigation](DEPENDENCY_D1_CLOSEOUT.md#debug-aot-signal-11)

## Focused macOS handoff

The D2 entry point is `ci-macos-package-compilation.yml`. It is manually
dispatched and passes only `^project_packages$` and targets `voidc`,
`voidls` and `void_project_packages_ir` to the shared qualification workflow.
The D1 entry point still selects
`^(project_manifest_header|project_manifest|project_dependencies)$`. The shared
job requires a GitHub-hosted ARM64 `macos-15` runner, Xcode 26.3 and
LLVM/Clang/LLD 23.1.4. The existing bootstrap verifies `ld64.lld` 23.1.4
and keeps Xcode's native Mach-O linker for compiler and AOT links. It uses
the pinned HTTP and host provider verification and native check, JIT and
AOT smoke. It keeps only bounded
diagnostics on failure for seven days. Runner image or SDK availability is
checked when the job starts. `actionlint` and embedded shell syntax checks
passed locally. Mocked runs verified the SHA, event and runner guards and
confirmed that an unexpected selection prevents CTest execution and a CTest
failure survives log capture. A passing native run qualifies the executed
D2 scope.
[GitHub-hosted runner labels](https://docs.github.com/en/actions/reference/runners/github-hosted-runners)

After Golden Wrench review, integrate the exact reviewed sources and workflows
into the default branch with `[skip ci]` in the integration commit message,
following the D1 integration precedent. The repository's global workflow
subscribes to `push` and `pull_request`, so the skip marker belongs on the
published integration commit. GitHub leaves any required skipped checks
pending. Once the workflow is present on the default branch, select a ref
pointing at the reviewed commit and dispatch the D2 workflow with that full
SHA. The job checks the dispatch SHA and checkout against this input.
[GitHub skip rules](https://docs.github.com/en/actions/how-tos/manage-workflow-runs/skip-workflow-runs) · [Manual dispatch rules](https://docs.github.com/en/actions/how-tos/manage-workflow-runs/manually-run-a-workflow)

```sh
reviewed_sha=$(git rev-parse HEAD)
gh workflow run ci-macos-package-compilation.yml --ref main -f reviewed_sha="$reviewed_sha"
```

## Linux reproduction

Run from the repository root with a private `TMPDIR` and a disposable build
directory. Use `Release` or `Debug` for both `configuration` and `provider_profile`
with `sanitizers=OFF`. For strict use `RelWithDebInfo`, `strict` and `ON`.
Export `ASAN_OPTIONS=halt_on_error=1:abort_on_error=1:detect_leaks=1` and
`UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1` for strict.

```sh
sdk="$PWD/.toolchains/llvm-23.1.4"
cmake -S . -B "$build" -G 'Unix Makefiles' \
  -DCMAKE_BUILD_TYPE="$configuration" -DBUILD_TESTING=ON \
  -DVOID_SANITIZERS="$sanitizers" -DVOID_LLVM_RESEARCH=OFF \
  -DCMAKE_C_COMPILER="$sdk/bin/clang" -DCMAKE_CXX_COMPILER="$sdk/bin/clang++" \
  -DLLVM_DIR="$sdk/lib/cmake/llvm" -DVOID_CLANG="$sdk/bin/clang" \
  -DVOID_LINKER="$sdk/bin/ld.lld" \
  -DVOID_HOST_DEPS_ROOT="$PWD/.toolchains/host/prefix/$provider_profile" \
  -DVOID_HTTP_DEPS_ROOT="$PWD/.toolchains/http/prefix/linux-aws-lc-5.12.0/$provider_profile"
cmake --build "$build" --parallel 3 --target voidc voidls void_project_packages_ir \
  void_project_header void_project_manifest void_project_selection void_project_sessions \
  void_native_fixture void_project_creation void_runtime_callbacks void_g5_reload void_globals
ctest --test-dir "$build" --parallel 2 --output-on-failure --no-tests=error \
  -R '^(project_packages|project_manifest_header|project_manifest|project_dependencies|project_selection|project_sessions|project_protocol|project_creation|project_creation_process|local_libraries|runtime_callbacks)$'
ctest --test-dir "$build" --parallel 2 --output-on-failure --no-tests=error \
  -R '^(project_packages|project_creation_process|debug_cache_processes|ffi_g4_conventional|ffi_g5_callbacks|stage11_globals)$'
```

## Changed files and Git status

| Change against D1 | Paths |
| --- | --- |
| Modified | `.github/workflows/ci-macos-dependencies.yml`, `CMakeLists.txt` |
| Modified | `include/void/frontend.hpp`, `include/void/ir.hpp`, `include/void/path.hpp`, `include/void/project_resolution.hpp` |
| Modified | `src/frontend/compiler.cpp`, `src/frontend/project_resolution.cpp`, `src/frontend/project_selection.inc` |
| Modified | `src/ir/interfaces.inc`, `src/ir/module.cpp`, `src/lsp/main.cpp`, `tests/project/dependencies.py` |
| Added | `.github/workflows/ci-macos-package-compilation.yml`, `.github/workflows/ci-macos-package-qualification.yml` |
| Added | `tests/project/packages.py`, `tests/project/packages_ir.cpp`, `docs/projects/DEPENDENCY_D2_CLOSEOUT.md` |

At the end of qualification, `main` and `origin/main` pointed to the D1
baseline and the index was empty. The D2 integration is a single local
commit on that parent.
The task-owned qualification workspace was removed. Available bytes on the
workspace filesystem were 367,980,032,000 before removal and 371,679,510,528 after
removal. Existing retained workspaces, toolchains and external archives
were kept.
