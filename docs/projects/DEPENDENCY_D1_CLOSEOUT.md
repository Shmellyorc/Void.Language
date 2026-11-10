# D1 local dependency resolution closeout

Implementation at baseline `9902abee531332bc56bec6fbf451fa23a6e5bb38`.
Golden Wrench provisionally accepted the D1 implementation. D1 resolves and
verifies local source projects. The selected lock records the entire declared
graph, including currently unused edges. [Reviewed design](PROJECT_DEPENDENCY_RESOLUTION_DESIGN.md)

## Manifest and graph

Schema 1 accepts `[dependencies.<alias>]` with required string fields `id`,
`path` and `version`. The alias uses a lowercase non-keyword compiler identifier
of up to 128 bytes. The ID must equal the selected library's `package.id`.
The selected version is the library's effective `package.version`, which
defaults to `project.version`. Application and project identities retain their
existing roles. A consumed library supplies `package.entry` as an exact,
package-relative `.void` file in its admitted tree. D1 checks the file boundary.
Public import and declaration validation belong to D2. Standalone library
checking can omit `package.entry`.

An exact `=SEMVER` includes prerelease and build metadata in equality.
Ranges intersect up to 16 comma-separated `>`, `>=`, `<` and `<=` bounds
over stable versions. A constraint is at most 1,024 bytes. Range endpoints
omit build metadata, while stable candidates with build metadata compare by
precedence. The parser keeps the
strongest lower and upper bounds and rejects empty intervals. A path selects
one candidate. The graph walks sorted aliases and shares a package only when
its ID and locator agree and all incoming constraints accept its version.
Origin and version conflicts identify both selections when available. A cycle
diagnostic gives the closing chain.

A locator is relative to its declaring project. Each step uses exact spelling
under the selected root. A parent step may reach a sibling within that root.
A descendant step may select a nested project at the end of the locator.
Crossing an intermediate manifest boundary fails, including when that
manifest's syntax is invalid. Locator components reject `.git`, `.void`,
`build` and `void.lock` by ASCII case fold. A locator is limited to 4,096
bytes and 64 components. Symlinks, reparse points and
hard-linked inputs fail admission. The budgets are 256 packages, 1,024 edges,
64 graph and tree levels, 131,072 directory entries per capture, 8,192 files
and boundary markers per package, 64 MiB per package and 256 MiB across package
trees. The existing manifest cap is 256 KiB. Lock reads cap at 1 MiB.
[Manifest parser](../../src/frontend/project.cpp) · [Resolver](../../src/frontend/project_resolution.cpp)

## Content identity and lock format

A framed field is an unsigned 64-bit big-endian byte count followed by the exact
bytes. Tree SHA-256 starts with the framed ASCII tag `void-package-tree-v1`.
Visit records in bytewise order by their portable, slash-separated paths
relative to the package. A file record frames `file`, the path and the raw
file bytes. A nested project directory contributes a record framing `boundary`
and its path. An exact regular nested manifest marks that boundary even if its
contents cannot parse. The enclosing tree does not hash its descendants.
A separately selected package receives its own digest. At the package top
level, ASCII-case-folded `.git`, `.void`, `build` and `void.lock` are
excluded. Ordinary directories contribute no record. File timestamps,
permissions, inode numbers and checkout paths are absent. The tree includes
the manifest and any other admitted regular files. Its companion manifest
digest is plain SHA-256 over the raw manifest bytes.

`void.lock` uses UTF-8/LF and canonical TOML. It starts with `format = 1` and
`resolver = 1`. The `[root]` table records `id`, `kind` and `inputs`.
The `inputs` digest frames `void-dependency-inputs-v1`, the root project ID
and kind, followed by the alias, package ID, canonical root-relative locator
and normalized constraint of each root edge in alias order. It ignores root
manifest formatting, declaration order and unrelated source bytes.
`[root.dependencies]` maps aliases to package IDs. Each sorted `[[package]]`
records `id`, `path`, `version`, `manifest` and `tree`, followed by a
`[package.dependencies]` alias map in bytewise order. All SHA-256 digests
use lowercase hexadecimal. The reader checks the schema, then compares
canonical bytes against a graph recomputed from admitted manifests and trees.

| Command | Result |
| --- | --- |
| `voidc resolve --project .` | Creates a missing lock for a project with dependencies. Verifies an identical lock without rewriting it. A stale or tampered lock requires explicit refresh. |
| `voidc resolve --project . --locked` | Verifies the lock without writing. Reports a missing lock when the project declares dependencies. |
| `voidc resolve --project . --refresh` | Recomputes the graph and explicitly replaces a differing, structurally valid lock. Malformed, conflicting and unsupported locks are reported and preserved for inspection. |

Dependency-free projects with no lock need no setup. The resolver reads local
metadata and file bytes, with no network, subprocess, native loading or Runtime
authority. Compilation continues to reject manifest-selected projects declaring
dependencies until D2. Dependency-free project discovery, local `lib/` imports,
stdlib imports and standalone file behavior retain their existing entry points.

Publication reuses project creation's held-directory and private staged-file
primitives. POSIX serializes writers on the root directory. Windows uses a
held writer file under `.void`. The writer flushes the staged file, recaptures
the inputs and checks the previous destination before atomic publication.
Readers see a complete old or new lock. A process killed during publication
can leave private staging under `.void`. A retry uses a new stage. Recapture
detects observed input changes and protects against path aliases. It supplies
neither an operating-system snapshot against simultaneous external edits nor
a power-loss durability guarantee. [Publication](../../src/frontend/project_creation.cpp)

## Final qualification

| Native platform and profile | Final replay |
| --- | --- |
| Linux x64 GNU Release | 33/33 selected tests passed. |
| Linux x64 GNU Debug with LLVM research | 33/33 selected tests passed on the fresh build. |
| Linux x64 GNU fatal ASan/UBSan with leak detection | 33/33 selected tests passed. The pinned RTTI-off LLVM SDK excludes the vptr check. |
| Windows x64 Release and Debug | UNQUALIFIED. VirtualBox confirmed a running Windows 10 guest, build 19045.6466, with Guest Additions active. Guest Control rejected login with `VBOX_E_IPRT_ERROR (0x80bb0005)` before the guest command started. Display capture returned `NS_ERROR_FAILURE (0x80004005)`. |
| macOS | UNQUALIFIED. Qualification was skipped because no macOS VirtualBox guest was available. |

Native Windows path admission, publication and Windows/Linux byte comparison
remain open. The D1 Windows fixture uses a junction and hard links. Guest
access failed before the file tests, independently of any symlink privilege
requirements in the wider suite.

The manifest suite covers the new fields, exact and bounded SemVer,
prereleases, optional standalone entry and remote-field rejection. The D1
process suite checks direct, transitive and diamond graphs, independent SHA-256
framing, canonical lock order across relocation and creation order, timestamp
and empty-directory independence, malformed and tampered locks, explicit refresh,
and dependency-free and standalone behavior. Negative fixtures cover wrong
ID, kind, version and origin, cycles, entry boundaries, casing, escapes, aliases,
depth, file and byte limits. Parallel writers exercise exclusive publication.
The Linux fault hook forces replacement failure and kills a writer immediately
before the final replacement, then verifies the old bytes and successful retry.
[Manifest tests](../../tests/project/manifest.cpp) · [Process tests](../../tests/project/dependencies.py) · [Linux fault hook](../../tests/project/dependency_lock_fault.c)

An independent directory snapshot check compared names, bytes, inodes, modes,
mtime and ctime before and after each read-only operation on all three Linux
builds. The dependency-free case, missing and stale `--locked` failures,
matching-lock verification, and the D2 compile guard preserved those
observations. A source edit invalidated the lock and an explicit refresh
updated it. The same fixture produced byte-identical initial locks in all
three Linux profiles. Atime was outside the snapshot because ordinary reads
can update it.

### Debug AOT signal 11

The initial complete D1 Debug selection passed 32/33. The harness recorded
status `-11` and empty stdout/stderr at line 104 of `project_creation_process`
while `debug_cache_processes` was running in the other CTest slot. From the
test's private working directory, the failing argv was
`<old-scratch>/debug/voidc build --project "$PWD/arbitrary" -o "$PWD/generated"`.
It was the second AOT build to the same output. The test had published and
launched the starter executable, rewritten the starter to compare
`os.data_dir()` with `os.data_dir(app_id)`, and run the revised source in both
JIT profiles. The failure happened in `voidc` before the subsequent launch.
The isolated replay and the pre-D1 control in that qualification pass both
passed. Release and fatal sanitizer selections each passed 33/33.
[Original subprocess sequence](../../tests/project/creation.py#L80)

The AOT path shares the manifest parser and project capture with JIT.
The failing fixture declares no dependencies, so it passes the D1 guard
without entering the resolver or lock writer. Only the CLI run branch
constructs a `DebugCache`. AOT verifies its native resources, lowers the
project in a new LLVM context and target machine, then invokes the driver
and publishes from a staged directory. A failed driver invocation is
translated into a compiler diagnostic. The harness starts a fresh process
for each compiler call. Previous calls can leave disk state, including the
JIT cache and destination executable. The backend, AOT builder, Runtime
and original harness have no D1 diff. Both Debug compilers emit `VOID5012`
for the generated project's default release profile. The CLI writes that
warning after manifest analysis and before calling `build_executable`.
The original empty stderr puts the observed failure before or during that
diagnostic in the reproduced command path. It supplies no stack or exact
fault location.
[CLI dispatch](../../src/main.cpp) · [Project selection](../../src/frontend/project_selection.inc) · [AOT builder](../../src/native_build.cpp#L220) · [LLVM emission](../../src/backend/llvm/backend.cpp#L2632)

The final pass configured fresh Release, Debug and strict build directories
against the pinned SDK and attested provider inputs. It used the original
33-test selector, two CTest jobs, the maintained registrations, the checkout
stdlib, and a task-owned XFS `TMPDIR`. CTest set `PYTHONDONTWRITEBYTECODE=1`.
The creation harness kept its 80-second
subprocess timeout and the registered CTest case kept its 300-second timeout.
The original failing binary and fixture had been removed under the retention
policy. The replays used freshly generated fixtures with the same sequence,
including the space-bearing paths and generated application IDs.

| Bounded crash control | Result |
| --- | --- |
| Fresh D1 Debug, original full CTest selection | 33/33 passed, including `project_creation_process`. |
| Fresh D1 Debug, creation and cache process tests in the same two-job CTest invocation, repeated three times | 2/2 passed in each invocation. |
| Fresh D1 Debug, isolated registered creation test | 1/1 passed. |
| Retained pre-D1 Debug, same creation launcher twice alone and once with the cache process test | All three creation runs passed their 64 subprocess checks. The concurrent cache test also passed. |
| Focused subprocess sequence, 28 fresh fixtures per Debug compiler | Both compilers passed all 28 fixtures and 52 AOT builds each. No subprocess signals or cores were observed. |

The focused matrix replayed lines 80–105 on four serial XFS fixtures and
eight XFS fixtures with two workers. Four more two-worker XFS fixtures added
`--no-cache` to the preceding JIT invocations while retaining the first AOT
publish. Four isolated XFS fixtures built the modified starter directly to
a fresh destination with no prior JIT or AOT invocation. Four two-worker
fixtures replayed the normal sequence with the project and link staging on
tmpfs. The final four repeated it with the child `TMPDIR` also on tmpfs.
Each AOT used the default optimized profile, and each revised linked program
had to return 31.
The focused replay allowed process-local core files in disposable fixture
directories and was set to capture a GDB backtrace on a signal. The original
kernel-journal lookup returned no matching entry, and kernel-buffer reads
were denied to this user.

The retained pre-D1 and fresh D1 runs used LLVM/Clang/LLD 23.1.4.
Each tested build's compiler, language server, native resource manifests,
Runtime archive and fixture archive passed SHA-256 comparisons before and
after the runs.
The original signal remains an unresolved intermittent observation. The
replays produced no fault to diagnose or justify a production correction.

Use LLVM/Clang/LLD 23.1.4, Python 3.12 or newer, and profile-matched attested
host and HTTP inputs. Configure in a task-owned disposable directory with
`BUILD_TESTING=ON`. Use Release, Debug with `VOID_LLVM_RESEARCH=ON` and
RelWithDebInfo with `VOID_SANITIZERS=ON`. For strict, export
`ASAN_OPTIONS=halt_on_error=1:abort_on_error=1:detect_leaks=1` and
`UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1`. Use a private
`TMPDIR` on POSIX or private `TEMP` and `TMP` on Windows.
[Configure instructions](../SOURCE_COMPILER.md#building-the-compiler) · [Retention policy](VOID_ARCHIVE_AND_RETENTION_POLICY.md)

```sh
export VOID_TEST_STDLIB="$PWD/stdlib"
cmake --build "$build" --parallel 2 --target voidc voidls void_project_header \
  void_project_manifest void_project_selection void_project_sessions \
  void_project_creation void_project_application void_tests void_cache_tests \
  void_runtime_callbacks void_g5_reload void_g4_aot_normal \
  void_g4_aot_optimized void_g4_aot_startup
ctest --test-dir "$build" --parallel 2 --output-on-failure \
  -R '^(project_|modules($|_)|local_libraries$|debug_cache($|_)|automatic_cache_processes$|runtime_callbacks$|ffi_g[45]_|cli_contract$|jit$|reload$|gc$)'
```

On native Windows, build the same targets except the three Linux G4-L AOT
targets and run the selector once per profile. Use `ctest -C Release` or
`ctest -C Debug` for multi-config builds. The resolver suite exercises the
portable graph and publication cases. The replacement fault hook runs on Linux.

For the registered Debug crash controls, use the Debug build as `build` and
keep the same environment. The retained pre-D1 Debug binaries can be passed
directly to the maintained launcher so it writes into scratch.

```sh
for pass in 1 2 3; do
  ctest --test-dir "$build" --parallel 2 --output-on-failure \
    -R '^(project_creation_process|debug_cache_processes)$'
done
ctest --test-dir "$build" --parallel 1 --output-on-failure \
  -R '^project_creation_process$'
python3 -B tools/test_output.py project_creation_process \
  python3 -B tests/project/creation.py "$pre_d1/voidc" "$pre_d1/voidls" \
  '@VOID_TEST_OUTPUT@/project-creation-evidence.json'
```

## Changed files and Git state

- `include/void/project.hpp` and `src/frontend/project.cpp` extend the manifest
  model and reuse its private TOML parser to validate the lock schema.
- `include/void/project_resolution.hpp`, `src/frontend/project_files.hpp` and
  `src/frontend/project_resolution.cpp` add the resolver, captured graph and
  canonical lock writer.
- `src/frontend/project_creation.cpp` reuses platform publication primitives for
  lock replacement and bounded held-file reads.
- `src/main.cpp` and `src/frontend/project_selection.inc` expose resolution
  and keep dependency compilation gated.
- `CMakeLists.txt`, `tests/project/manifest.cpp`, `tests/project/dependencies.py`
  and `tests/project/dependency_lock_fault.c` register and exercise D1.
- `docs/projects/DEPENDENCY_D1_CLOSEOUT.md` records this handoff.

Git HEAD remains at the baseline on `main` with an empty index diff. Seven
tracked files have D1 working-tree modifications. The five new D1 source and
test files and this closeout are untracked. This final pass changed only the
closeout. The pre-existing untracked material, including the approved design
report, is outside the D1 changes. Qualification used task-owned disposable
paths and preserved the eight retained roots, the historical toolchains and
archives, and the S1–S7 backup. Both qualification workspaces were removed.
Filesystem free space went from 371,977,961,472 to 376,720,982,016 bytes
during final cleanup.
