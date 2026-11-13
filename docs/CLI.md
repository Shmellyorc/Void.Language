# voidc command reference

## Project creation (Slice 5)

```text
voidc init [DIR]
voidc new PATH
voidc init . --name Orbit --app-id org.example.orbit
voidc new orbit --source src/game.void --app-id org.example.orbit
voidc new geometry --kind library --package-name geometry
```

Init writes a manifest in an existing admitted directory and defaults to the
current directory. New publishes a fresh directory with a manifest and a starter.
Both accept `--kind application|library`, `--name TEXT`, `--version SEMVER`,
`--project-id ID`, and `--nested`. Use `--app-id ID` for applications or
`--package-id ID` and `--package-name IDENTIFIER` for libraries. New also accepts
`--source PATH`. The application and version defaults are `application` and
`0.1.0`. Names default to the directory basename subject to the manifest validators.
Generated project and role IDs use separate host entropy draws. The commands preview
the manifest path, IDs and next check command before publication.

Init preserves existing sources and works before a unique main has been written.
New uses `src/app.void` for applications or `src/<package-name>.void` for libraries
by default. A custom source path sets the source root to its parent. Neither command
writes an entry override. Publication refuses existing or wrong-case destinations,
and a child of a manifest project requires `--nested`. Completed creation returns
0, validation or filesystem failures return 1, and malformed usage returns 2.
Use `voidc resolve --project .` for available declared dependencies and
`voidc resolve --project . --locked` to verify the existing lock. See
[Slice 5](projects/SLICE5.md) and the [dependency closeout](projects/DEPENDENCY_D2_CLOSEOUT.md).

The following sections retain the scope of their original implementation slices.

## Inspect project dependencies

```text
voidc deps explain --project .
voidc deps explain --project . --json
```

The compiler verifies the existing lock and local inputs before displaying a
graph. `Direct` rows start at the selected project and `Transitive` rows start
at a package. For example, a direct alias `geometry` can introduce `units`,
while another direct alias `algebra` also introduces `units`. The report has
one package record for `units` and shows both parent relationships. Package
records include the verified version, entry, manifest and tree SHA-256, and
local path or pinned Git origin, revision and subdirectory. Relationships
include each alias, version requirement and declared source.

The command reads project and lock inputs and the existing local snapshots.
It performs no Git acquisition, lock publication, cache update or package
execution. A dependency-free project with no lock reports that a lock is
unnecessary. For a project with an existing lock, changing a manifest alias
or a local package input produces the resolver's stale-lock diagnostic.
Review the changes before choosing an explicit command.
Removing a required transitive record from a previously valid lock also
produces the stale-lock diagnostic. A missing Git snapshot identifies the
declared origin and exact revision in its diagnostic.

```text
voidc resolve --project .
voidc deps fetch --project .
voidc deps fetch --project . --allow-git https://example.org/units.git 0123456789abcdef0123456789abcdef01234567
voidc resolve --project . --refresh
voidc resolve --project . --locked
```

Use ordinary `resolve` to create a missing lock when inputs are available.
Use `deps fetch` when a declared Git snapshot is missing and supply the exact
origin and revision for any transitive pin that needs approval. The origin and
revision above illustrate the argument format. Use `resolve --refresh` to
publish reviewed manifest or package changes, then verify the result with
`resolve --locked` or `deps explain`. A corrupt Git snapshot needs a verified
replacement or a clean project copy with explicit acquisition. A malformed
lock must be reviewed and moved aside before ordinary resolution. Correct
wrong IDs, missing entries, cycles and origin conflicts in the declared
project inputs before running resolution again.

`--json` writes one UTF-8 object with `schema: 1`. On success, `status` is
`"ok"` and the remaining fields are:

- `project_id` and `lock`, which has `status: "verified"` and a `sha256` field,
  or `status: "not_required"`.
- `packages`, ordered by ID. Each record has `id`, `version`, `entry`,
  `manifest_sha256`, `tree_sha256` and `source`.
- `relationships`, ordered by direct aliases, then by package ID and
  that package's aliases. Each record has `direct`, `parent`, `alias`, `target`,
  `version_requirement` and `source`. `direct` distinguishes a project ID
  from a package ID in `parent`.

A local `source` has `kind: "local"` and `path`. A package path is relative to
the project root. A relationship path is relative to its declaring manifest.
A Git `source` has `kind: "git"`, `origin`, `revision` and `subdir`.
Success exits 0. An inspection failure exits 1 and prints an ordinary diagnostic
on stderr, or an object with `schema`, `status: "error"` and `message` on stdout
in JSON mode. Malformed invocations exit 2. Output is bounded to 4 MiB before
writing. Native-provider inputs participate in lock verification when declared.
The graph report describes source packages.

## Manifest project profiles (Slice 3)

`--profile debug|release` is project-only on check/run/build/emit/package. An explicit
source file keeps legacy semantics and rejects this option with usage status 2.
Default project selection is release; manifest settings apply, then `--no-opt`.
`--cache` may replace an implicit release default with debug, but cannot override
an explicitly selected release profile. It requires effective debug optimization
`none`; a custom debug/O2 setting therefore conflicts unless explicit `--no-opt`
disables it. `--no-cache` remains run-only and changes persistence, not optimization.
Conflicts, duplicate profile options and unknown profile names are usage errors.

Both profiles default to `debug_info="none"`. Generated manifests can omit the
build tables. On Linux x64 GNU and Windows x64 MSVC, project `emit` and `build`
apply the selected mode.

| Mode | VOID source metadata |
| --- | --- |
| `none` | Omits optional VOID source debug information. Runtime failure locations and native unwind information remain available. |
| `line` | Emits DWARF source locations and subprogram ranges for breakpoints and stepping. |
| `full` | Adds type information and, in O0 builds, names and locations for supported scalar parameters and function-level locals. Inspect these values after their declarations execute. |

Windows embeds DWARF in the PE executable with the matching SDK's
`lld-link.exe`. The executable remains a single published file. PDB-based
debuggers require a separate contract. Linux emits DWARF in ELF. Both formats
retain physical source paths, including spaces. A debug build may reveal the
checkout path in its metadata.

Debug information and optimization are independent. Debug defaults to O0 and
Release to O2. O2 may inline or remove source statements, and the current
optimized build does not emit local variable locations. The selected mode
participates in profile and build provenance. Explicit `line` and `full` are
quiet on supported project check/build/emit. JIT, package and unsupported
native targets report **VOID5012** for these requests. Standalone commands
keep their existing behavior. [Source debugger setup](EDITOR_INTEGRATION.md#native-aot-source-debugging)

The conservative project selection digest plus effective profile settings belongs
to build/cache provenance. It is distinct from semantic project context and the
future Runtime application identity. Every project invocation still revalidates
selection before cache use. See [Slice 3 qualification](projects/SLICE3.md).


Slice 1's compiler-library manifest parser is now used by Slice 2's compiler-owned
project selection. [Slice 1 evidence](projects/SLICE1_CLOSEOUT.md) remains historical;
[Slice 2 engineering and platform limits](projects/SLICE2.md) describe the current
local implementation. Bare explicit files and manifestless directories retain
their legacy root, classification, argument, JIT and AOT behavior.

A directory operand (or run/build cwd) with `void.project.toml` selects that exact
manifest. No parent search or fallback after invalid configuration occurs. An
application automatically selects one eligible main in any filename, with no
main.void preference; missing/competing/wrong-signature mains diagnose. Only the
selected import graph gets semantic validation and global initialization. Libraries
check their admitted source union without an application entry; project run/build/
emit of libraries is unsupported.

Implemented selectors:

```text
voidc check --project .
voidc run --project . -- literal-application-argument
voidc build --project . --entry src/launcher.void
voidc run . --entry tools/script.void --no-opt
```

`--project DIR` conflicts with positional file/directory selectors. `--entry PATH`
requires a manifest context, is manifest-relative, overrides `project.entry` and
bypasses automatic seed scanning; invalid overrides never fall back. Bare explicit
files never acquire project context from an adjacent manifest. A library `--entry`
is a usage conflict. Paths cannot cross child project boundaries. API callers can
supply frozen named source and TOML overlays separately; Slice 3 integrates those
inputs into ToolingSession/voidls and VS Code without an editor-side resolver.

VS Code's Check, Build and Run commands invoke these same selectors and profile
flags on saved files. The editor defaults to the Debug profile. Application
arguments from the editor are passed after `--`. Use a terminal for explicit
native provider authorization. [Editor setup](EDITOR_INTEGRATION.md#vs-code-project-commands)

## Implemented command contract

This is the baseline public command contract. Its help/version output and
conventional `--` operand delimiter retain the existing source, entry, JIT/cache
and native-linking behavior. The selectors above add explicit manifest context;
the remaining project roadmap is future work.

```text
voidc --help | -h
voidc --version
voidc help [check|run|build|emit]
voidc <command> --help | -h
voidc check <file-or-directory> [--no-opt]
voidc run [file-or-directory] [legacy-i32-input] [--no-opt] [--no-cache | --cache]
voidc build [file-or-directory] [-o executable] [--no-opt]
voidc emit <file-or-directory> -o <object> [--no-opt]
```

| Command | Result | Example |
| --- | --- | --- |
| `check` | Analyze without executing startup. Bare sources/libraries need no app entry; automatic app projects require main. | `voidc check main.void` |
| `run` | JIT-execute the selected entry. | `voidc run .` |
| `build` | Compile/link a native application with matching Runtime/resources. | `voidc build . -o "output folder/hello"` |
| `emit` | Write a host-native object for manual linking. | `voidc emit main.void -o main.o` |

Run/build default to the current directory. Every directory argument selects its
exact `main.void` when no manifest is present; a selected manifest uses the
compiler-owned discovery above. Explicit file paths may use other names. Options can occur
before or after operands. Quote paths with spaces. For a source filename starting
with a dash, use `voidc check -- -example.void` or `./-example.void`. The delimiter
ends option parsing; put `-o` and other options before it. Empty and malformed
invocations fail visibly. Duplicate/unknown options reject. Help must be its own
invocation (`voidc run --help`); it does not suppress invalid trailing arguments.
`--version` is global and reports the development project version, LLVM version,
SDK default target and whole-compiler build identity, not a release support promise.
Help starts with the same compiler version, followed by LLVM, usage, commands
and options. Version reporting returns before standard-source discovery and needs
no source project, installed stdlib or native resource manifest.

## Entries, input and exit status

Application entry selection is automatic. Supported `main` or executable top-level
code runs as an application; globals initialize before entry. Applications take
no numeric CLI input. `--app` is removed and always rejected, including alongside
help. Existing legacy `entry(i32) -> i32` selection/precedence remains supported:
`voidc run calculation.void 10` accepts one signed decimal i32, prints its returned
value and exits zero. Its default input is zero. For cwd input, spell the path:
`voidc run . 10`. Application process status and printed numeric result differ.

| Status | Meaning |
| --- | --- |
| 0 | Successful command/help/version; successful legacy numeric adapter. |
| 1 | Compilation, source/discovery, output or toolchain failure. |
| 2 | Invalid CLI invocation. |
| Application status | `run` propagates the application's return value; OS process-status limits apply. |

An application can itself return 1 or 2; those numbers alone do not identify a
compiler failure. Native applications retain their existing argument/status policy.

## Optimization and cache

O2 remains the default. `--no-opt` disables it. `run --cache` selects Debug code
and prints cache status; `--no-cache` disables disk caching without disabling O2.
The two cache options conflict and are available only for run. Eligible Debug JIT
runs use the existing automatic source-root cache, with its existing safe
unavailable fallback. `check --no-opt` remains a compatibility no-op. Other
non-run commands reject cache options. Legacy cache policy and identity rules are unchanged. Project mode revalidates
its full applicable inventory/graph before lookup and adds a distinct selection
key binding manifest bytes, physical root, source membership/bytes, boundaries
and entry mode. A newly added second main cannot be hidden by a cache hit.
Rebuilding the changed driver
changes the whole-compiler fingerprint normally; old compiled cache entries are
not promised to survive that rebuild.

## Native output and installation

### Application distributions

`voidc package --project . --profile release` assembles a Linux x64 GNU or
Windows x64 MSVC application in `<project>/dist`. `-o DIR` selects another destination, with
missing parent directories created. Entry discovery and `--entry` use the
existing manifest rules. A declared managed native provider requires
`--allow-native` and a verified `void.lock` and local catalog.

```sh
voidc package --project . --profile release -o "output/My Application"
voidc package --project . --allow-native --replace
voidc package --verify "output/My Application"
```

The directory contains `app` on Linux or `app.exe` on Windows, the encoded `application.meta`, a
`void-package.txt` inventory, Runtime notices in `licenses/`, and the
managed `native/` payload when selected. Declare up to 32 non-source files
with `[application] resources = ["assets/message.txt"]`. These are flat,
portable ASCII `assets/` paths with a combined 16 MiB limit. A root `LICENSE`
is also included when supplied. Applications locate assets with
`os.resource_dir()`. `--verify` checks lengths, SHA-256, metadata, names and
permissions. Applications retain their own asset-use checks.

On Linux, `--replace` accepts an existing verified package of the same
application ID and publishes it by atomic directory exchange. Windows
requires a fresh destination, including when `--replace` is present. The
inventory records the selected input digest and actual file
digests. Equivalent input selections preserve the file membership and
selected data identities. Executable byte identity across build locations
has a separate qualification boundary.

Linux packages use OS-provided runtime libraries, including the IDN and
Unicode SONAMEs selected by the native link. The package executable has no
development RUNPATH. Required OS and display dependencies and qualification
limits are recorded in the [D5 closeout](projects/DEPENDENCY_D5_CLOSEOUT.md).
See the [Windows implementation record](projects/W2_WINDOWS_D5_IMPLEMENTATION.md)
for Windows package layout, admission, and qualification.

### Native build outputs

`build` defaults to `<entry-directory>/build/<entry-stem>` with `.exe` on Windows.
Manifest-selected builds instead default to `<manifest-root>/build/<entry-stem>`
with the same executable suffix. `-o` is caller-relative or absolute; absent parent
directories are created.
Output/source collisions, `.void` outputs and non-file destinations reject; failed
builds preserve an existing good executable. Emit does not link; use `.o` on Unix
or `.obj` on Windows, and supply matching Runtime/dependencies when linking by hand.

Native builds use `void-native.txt` and `void-native-resources.txt` beside the actual
compiler, installed archives under `lib/void` and the original Clang/LLD **23.1.4**
SDK. They do not search the caller's cwd for replacement manifests. Resource hashes
and tool versions are checked. Missing resources include a reinstall hint; linker
failure reports identify the driver and point to host development prerequisites.
See [installation](install/README.md), [troubleshooting](TROUBLESHOOTING.md),
[legacy source contract](SOURCE_COMPILER.md) and [native workflow](STAGE9_APPLICATION_WORKFLOW.md).

## Source packages: add and tidy

```sh
voidc add ALIAS [--project DIR] --path DIR
voidc add ALIAS [--project DIR] --git ORIGIN --rev FULL_SHA [--subdir DIR]
voidc tidy [--project DIR] [--check]
```

Both commands default to the current project. `--path` is relative to that
project; a path already inside it remains editable in place, while an external
source is installed as an editable copy under `libs/ALIAS`. Git pins must be
complete lowercase immutable commits, acquired through D3's existing transport.
There is no bare-name registry lookup. `add` selects an application manifest,
validates the library's package ID/version/entry, appends its dependency table
and maintains `void.lock`. Conflicts retain the previous project.

`use lib/...` selects ordinary project-owned source. `use pkg/...` selects a
declared and locked external package, including when its files are physically
installed under `libs/`. Local libraries do not require tidy or manifests.

Tidy checks relevant imports, retains unused dependencies with advice, refreshes
legitimate local locks and materializes verified Git source views. It never
fetches or upgrades Git pins. `--check` performs read-only consistency checking
and exits nonzero for required corrections; unused-package advice alone does
not fail CI. Native execution still requires the existing separate D4 approval.
See [local-library ownership and recovery](LOCAL_LIBRARIES.md#g26-installed-source-packages-and-maintenance)
and [G26 qualification](projects/G26_PACKAGE_MAINTENANCE.md).
