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
Resolve and package workflows remain in the project roadmap. See
[Slice 5](projects/SLICE5.md).

The following sections retain the scope of their original implementation slices.

## Manifest project profiles (Slice 3)

`--profile debug|release` is project-only on check/run/build/emit. An explicit
source file keeps legacy semantics and rejects this option with usage status 2.
Default project selection is release; manifest settings apply, then `--no-opt`.
`--cache` may replace an implicit release default with debug, but cannot override
an explicitly selected release profile. It requires effective debug optimization
`none`; a custom debug/O2 setting therefore conflicts unless explicit `--no-opt`
disables it. `--no-cache` remains run-only and changes persistence, not optimization.
Conflicts, duplicate profile options and unknown profile names are usage errors.

Only the selected profile's unsupported `debug_info="line"` or `"full"` emits the
owned structured warning **VOID5012**, including on project check. `none` is quiet.
The requested value remains in the immutable manifest. Profile-controlled debug
information modes are not implemented: existing backend source/type/location
metadata behavior is retained, so `none` does not newly strip those records and a
line/full request does not establish that its requested symbols were generated.
Standalone commands receive no new warning or behavior change. This limitation
and warning policy were explicitly approved for S3; actual mode-specific emission
remains a separate review milestone.

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

`--profile debug|release` is implemented in Slice 3 with the precedence and warning
contract above. The [Phase 1 ownership contract](PROJECT_ARCHITECTURE_ROADMAP.md#phase-1-ownership-and-data-flow)
retains future creation and resolver/package workflows. The build example is supported;
init/new examples remain proposed only:

```text
voidc build --project . --entry src/launcher.void --profile debug
voidc init . --name Orbit --app-id org.example.orbit
voidc new orbit --source src/game.void --app-id org.example.orbit
```

The [creation contract](PROJECT_ARCHITECTURE_ROADMAP.md#proposed-first-file-creation-and-terminal-workflows)
creates no dependency installation authority, never overwrites manifests or renames
sources, and omits entry by default. Detailed options remain proposed pending review.
Creation implementation awaits its later reviewed slice.

## Future project commands, 8 November 2026

The [project roadmap](PROJECT_ARCHITECTURE_ROADMAP.md) proposes `voidc init` and
`voidc new` and eventual resolve/package workflows. Those forms remain future
commands; manifest-aware check/run/build/emit and selectors are implemented in S2,
with project profiles added in S3. The approved entry direction requires no filename convention or mandatory
entry field: one valid main is selected by compiler analysis; ambiguity diagnoses.
The command forms below remain compatible; `main.void` directory discovery
applies when no manifest is selected. Explicit source paths and standalone/top-level/legacy
execution must remain compatible. Init/new perform no implicit dependency install.

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
