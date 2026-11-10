# D3 explicit Git source acquisition closeout

D3 extends the resolver and locked compiler from
`269770646c25b18e27534dbe47ce4ae0343896da`. Acquisition, lock publication
and compilation remain separate operations.
[Dependency design](PROJECT_DEPENDENCY_RESOLUTION_DESIGN.md) · [D2 closeout](DEPENDENCY_D2_CLOSEOUT.md)

## Declaring and acquiring a Git package

```toml
[dependencies.geometry]
id = "org.example.geometry"
git = "file:origins/geometry.git"
rev = "0123456789abcdef0123456789abcdef01234567"
subdir = "packages/geometry"
version = "=1.4.0"
```

A dependency uses either `path` or `git` with a full lowercase SHA-1 commit ID
in `rev`. `subdir` is optional for Git and defaults to the repository root.
It uses exact, portable repository-relative components. The local `file:`
origin is relative to the selected project root, even in a transitive
manifest. Acquisition accepts bare repositories or ordinary worktrees with an
actual `.git` directory. The existing package ID, version, library kind,
entry and graph conflict rules apply. A local path edge declared inside a
Git snapshot stays inside that pinned repository.

```sh
voidc deps fetch --project .
voidc resolve --project .
voidc check --project .
voidc run --project .
```

The selected root authorizes acquisition of its directly declared pins.
To acquire a missing pin reached transitively, explicitly add
`--allow-git ORIGIN REV` to `deps fetch`, unless the root already declares the
same pin. Repeated flags authorize multiple pins. Acquisition walks the
existing graph and checks package IDs, versions, boundaries and entries before
publishing new snapshots. It leaves the lock to `resolve`. After changing a
pin, explicitly fetch it and use `resolve --refresh`. A matching lock can be
checked with `resolve --locked`.

HTTPS declarations accept plain credential-free origins. HTTPS acquisition
is unavailable in this slice. Mutable refs, tags as locators, alternate
protocols, linked Git worktree metadata and alternate or promisor object
stores are unsupported.

## Identity, storage and offline use

Only `deps fetch` runs the absolute Git executable chosen through
`VOID_GIT_EXECUTABLE` at compiler configuration. It asks `git cat-file` for
the pinned commit and its raw tree and blob objects. Git runs with a private
repository configuration, a scrubbed environment and the explicitly admitted
local object database. Acquisition does not consult origin configuration,
use an origin worktree, materialize submodules, or apply filters and export
attributes. The Git object format distinguishes SHA-1 commit identity from
VOID's SHA-256 source identity. [Git batch object access](https://git-scm.com/docs/git-cat-file)

The project owns `.void/git-deps`. A key derived from origin and revision
selects a directory containing one SHA-256-addressed snapshot. The snapshot
holds raw commit and tree proof records plus the exported files. VOID
recomputes the Git object IDs, checks the raw tree modes, names, case and
inventory, and verifies the SHA-256 snapshot identity before use. The
existing resolver independently computes each selected package's D1 tree
digest with its original framing, exclusions and nested project boundary
markers. A Git snapshot is bounded to 64 MiB of files, 8 MiB of commit and
tree proof, and 8,192 entries. A graph is bounded to 256 MiB of retained Git
file bytes, proof and path text in addition to the existing resolver limits.

Publication writes to a private sibling stage, verifies it, and exclusively
renames the completed directory. A concurrent winner is reused only after
verification. A killed writer can leave an `acquire-*` stage that readers
ignore. An absent pin can be reacquired with an explicit fetch. A corrupt
published pin fails closed. Move that damaged cache entry aside before
fetching it again. A complete pin published before a later publication
failure can be reused. Read-side verification protects admission if stored
files are incomplete after an interruption.

Local-only graphs keep the byte-identical D1 lock format. Graphs containing
Git use format and resolver revision 2. Git package rows record `git`,
`rev` and canonical `subdir` separately from package version, manifest
SHA-256, tree SHA-256 and outgoing edges. The lock contains no physical
checkout or cache path.

`resolve`, `check`, `run`, `build`, `emit` and editor analysis use verified
snapshots without invoking Git or acquiring data. The D2 ownership model
admits held bytes to SourceGraph and typed IR. Its dependency-plan identity
continues into the JIT cache, AOT provenance and Runtime generation
compatibility. The AOT executable runs independently of the checkout,
dependency store and compiler. Git metadata supplies no native-provider
authority. [Resolver](../../src/frontend/project_resolution.cpp) · [Git snapshot](../../src/frontend/project_git.cpp)

## Qualification

The Linux builds used LLVM/Clang/LLD 23.1.4 and the pinned, profile-matched
host and AWS-LC 5.12.0 HTTP provider inputs. The maintained D3 fixture covers
packed objects, moved refs and dirty worktrees, hostile Git configuration,
direct and transitive pins, explicit transitive authorization, a shared
diamond, mixed local/Git locks and repository-relative local edges. It
compares the D1 digest against independently framed blob bytes and exercises
both JIT profiles, linked AOT, cache invalidation, relocation and offline
operation. Negative cases cover wrong revisions and object types, package
ID and version mismatch, missing and corrupt snapshots, proof and inventory
tampering, paths and enclosure escapes, unsupported Git entries and
origin/revision conflicts. It exercises concurrent acquisition and, on
Linux, kills a publisher just after it creates the private stage. The retry
ignores that stage and completes with the original lock. Power-loss
durability and native publication on macOS and Windows remain untested.

| Native profile | Result |
| --- | --- |
| Linux x86-64 Release | 10/10 selected tests passed |
| Linux x86-64 Debug | 10/10 selected tests passed |
| Linux x86-64 fatal ASan/UBSan with leak detection | 10/10 selected tests passed |
| Windows Release and Debug | UNQUALIFIED. VirtualBox Guest Control rejected the Windows 10 login before launching the guest command. |
| macOS ARM64 Release and Debug | UNQUALIFIED for D3. The workflow is prepared locally. |

An early D3 object reader reused a redirected output file without truncating
it. The first local fixture exposed the mismatch and the reader now clears
the file before each Git invocation. An early D1 manifest regression exposed
a missing diagnostic hint for a mixed locator. The final parser restores the
hint and preserves the original assertion. The strict build follows the
pinned LLVM SDK's RTTI-off configuration.

A disposable `voidls` probe passed on all three Linux builds. Definition
navigation resolved to the acquired file with the origin removed. After
removing the snapshot, refresh reported the missing pin and left the store
empty. The historical D1 Debug AOT signal 11 remains unresolved and did
not recur in the selected project-creation tests.
[D1 crash investigation](DEPENDENCY_D1_CLOSEOUT.md#debug-aot-signal-11)

For integration, a fresh Release build passed all five selected D1–D3
registrations. A disposable probe also acquired from a bare repository,
emitted an object and navigated definitions with the origin absent, and
verified that missing-snapshot editor analysis left the store empty. The
package script's execution marker remained absent. A Git package manifest
declaring a native provider was rejected before snapshot publication.

## Reproduction and macOS handoff

Configure a disposable build directory with `BUILD_TESTING=ON`, the
profile-matched Linux prefixes and the pinned LLVM 23.1.4 compiler and
linker. Use `Release` or `Debug`, or `RelWithDebInfo` with
`VOID_SANITIZERS=ON` for strict. The D2 closeout records the full configure
command. To specify Git explicitly, add
`-DVOID_GIT_EXECUTABLE=/absolute/path/to/git` at configure time.
[Linux configure command](DEPENDENCY_D2_CLOSEOUT.md#linux-reproduction)

```sh
export TMPDIR="$scratch/tmp"
# Strict only
export ASAN_OPTIONS=halt_on_error=1:abort_on_error=1:detect_leaks=1
export UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1

cmake --build "$build" --parallel 3 --target voidc voidls void_project_packages_ir \
  void_project_header void_project_manifest void_runtime_callbacks void_g5_reload
ctest --test-dir "$build" --parallel 2 --output-on-failure --no-tests=error \
  -R '^(project_manifest_header|project_manifest|project_dependencies|project_packages|project_git_dependencies|local_libraries|project_creation_process|runtime_callbacks|ffi_g4_conventional|ffi_g5_callbacks)$'
```

The manual `ci-macos-git-dependencies.yml` passes only
`^project_git_dependencies$` and target `voidc` to the existing shared
workflow. It reuses native ARM64 `macos-15` Debug and Release, reviewed SHA
verification, LLVM/Clang/LLD 23.1.4 and pinned provider setup, the native
check/JIT/AOT smoke and bounded failure logs. As in D1/D2, the bootstrap
verifies `ld64.lld` and uses Xcode's native Mach-O linker. D1 and D2 selectors retain
their established scopes. Actionlint and Bash syntax checks passed. Mocked
shell runs verified the SHA, event and runner guards, the build arguments
and the exact CTest selection. A mismatched listing blocked execution and
a failed CTest run kept its failure status through log capture.
[Workflow](../../.github/workflows/ci-macos-git-dependencies.yml)

## Changed files and Git status

| Change | Paths |
| --- | --- |
| Modified | `CMakeLists.txt`, `include/void/project.hpp`, `include/void/project_resolution.hpp` |
| Modified | `src/frontend/project.cpp`, `src/frontend/project_creation.cpp`, `src/frontend/project_files.hpp`, `src/frontend/project_resolution.cpp`, `src/frontend/project_selection.inc`, `src/main.cpp` |
| Added | `src/frontend/project_git.cpp`, `src/frontend/project_git.hpp` |
| Added | `tests/project/git_dependencies.py`, `tests/project/git_snapshot_fault.c`, `.github/workflows/ci-macos-git-dependencies.yml`, `docs/projects/DEPENDENCY_D3_CLOSEOUT.md` |

At the end of qualification, `main` and `origin/main` pointed to the D2
baseline with an empty index. Integration uses one local commit on that
parent. The task-owned qualification workspace was removed. Filesystem availability was
368,867,680,256 bytes before removal and 371,549,220,864 bytes afterward. Retained
builds, toolchains, archives and backups were preserved.
