# voidc command reference

## Project creation (Slice 5)

```text
voidc init [DIR]
voidc new
voidc new list
voidc new app Orbit
voidc new lib geometry
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
by default. A custom source path sets the source root to its parent. New libraries
record that known path as `package.entry`; init does not infer an entry. Applications
retain automatic main discovery without a generated `project.entry` override.
Publication refuses existing or wrong-case destinations,
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
voidc help [COMMAND]
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

Top-level help lists every public command with space-padded columns. Each
command's own help lists its source, command and presentation options with
widths calculated from that section's spellings. `help COMMAND` accepts all
fourteen listed commands. Existing nested routes remain exact:

```text
voidc deps explain --help    # also -h
voidc deps fetch --help      # also -h
voidc native import --help
voidc package install --help
voidc package uninstall --help
voidc new install --help
voidc new uninstall --help
```

Use the parent command page for other nested operations. Help is not a token
that can be inserted into arbitrary argument vectors. In particular,
`native import -h` and `package install -h` retain their existing parser behavior;
literal application arguments after `run ... --` never become compiler help.

## Status and compiler timings (G75)

`check`, `run`, `build`, `emit`, and application assembly with `package` accept:

| Option | Behavior |
| --- | --- |
| `--status auto` | Default. Emit concise phase lines and a build total only when stderr is an eligible interactive terminal. CI and dumb/unknown terminals remain quiet. |
| `--status plain` | Explicit plain stderr status, including pipes and task terminals. |
| `--status off` | Suppress the new status lines. Existing diagnostics, cache messages and application output still apply. |
| `--timings` | Opt in to detailed inclusive wall timings on stderr. |
| `--timings-format text\|json` | Require `--timings`; default text. JSON requires a separate report file. |
| `--timings-output PATH` | Fresh JSON report destination; valid only with JSON timings. |

Default redirected stdout and stderr retain their existing contents. There are
no spinners, ANSI controls or animation threads. Manifest Debug/Release labels
describe the selected profile; bare source builds have no invented profile.
Run reports build completion before invoking the application. The application's
duration, return status and legacy numeric result are separate from compiler
work. Explicit timing output can accompany `--status off`.

```sh
voidc build --project . --status plain
voidc run --project . --no-cache --timings
# Create the reports directory first, outside the source project.
voidc build --project . --timings --timings-format json --timings-output /tmp/void-reports/build-1.json
```

JSON is written only to the explicitly selected file. The file must not exist;
its parent must already be an admitted ordinary directory. Reports cannot go
inside the selected source tree, a manifest project, the artifact tree, reserved
source/resource directories, package storage or native approval storage. Aliased
parents, reserved filenames and case collisions are refused. Use a distinct path
for each invocation. JSON mode emits no new status lines and never puts the
report on stdout or stderr.

The [version-1 JSON schema](schemas/cli-timings-v1.schema.json) describes actual
emitted reports. Durations use a monotonic clock and integer microseconds, rounded
once at serialization. Scopes are inclusive: nested phases overlap, and adding
them does not produce the total. Repeated scopes of one category aggregate;
same-category recursion is measured once. Combined lex/parse, object emission
and ORC materialization intervals reflect the compiler's real boundaries.
Unavailable phases have null durations and explicit states/reasons. Clock tick
metadata is nominal; effective resolution is uncalibrated and null. Authorization
wait, when reached, remains inside compiler wall time and is identified explicitly.

Invalid timing options or report destinations fail before compilation with exit
2. After report admission, ordinary compiler failures can produce partial reports.
A late report write/publication failure prints `Timing report failed:` on stderr,
preserves the compiler/application exit status, and leaves a successfully published
artifact valid. Process termination or a fatal signal can prevent report completion;
no signal handler fabricates a final report. Interrupted private staging may remain.

`voidc --capabilities` is a resource-independent metadata query for editor
integration. Schema 1 advertises `cliPresentation: 1`, `statusPlain: true` and
`timingsSchema: 1`. Integrations should probe before sending new flags to an older
compiler. [Implementation and qualification](projects/G75_CLI_EXPERIENCE_IMPLEMENTATION.md)

VOIDUP accepts global `--status auto|plain|off` before its command. The small
adapter reports fixed operation labels and a total for install/default/rollback/
verify/doctor. Run, which and list retain their existing forwarding, path and
inventory output. VOIDUP has no compiler timing-report interface. Its status
uses existing console/time facilities and grants no installation or trust authority.

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

## Built-in project starters

```sh
voidc new list
voidc new app --name MyGame
voidc new classlib --name MyMath
voidc run --project MyGame
voidc check --project MyMath
```

Both starters are built in. `lib` is a compatibility alias for
`classlib`. `voidc new app MyGame` and `voidc new PATH` still create
a fresh directory. A library starter has a public `answer()` function
and a library manifest. It uses the lowercase directory name as the
package name by default. Supply `--package-name` when the directory
name is unsuitable as a VOID identifier.

Use `voidc new app .` or `voidc new classlib .` from an empty current
directory. Hidden files count toward emptiness. Use `voidc init` to add a
manifest to existing sources.

Open a generated application folder in VS Code and choose **VOID: Run
Project**. Linux x64 application starters also include an LLDB DAP
launch configuration. Build the Debug executable first, then use **F5**
with the official LLVM extension and matching SDK configured. See the
[starter editor workflow](EDITOR_INTEGRATION.md#running-a-built-in-application-starter).

## Custom project templates

```sh
voidc new install --dir tests/project/template_fixture
voidc new list
voidc new voidgame --name MyAwesomeGame
voidc run --project MyAwesomeGame
voidc new uninstall voidgame
```

Install directly from an HTTPS Git repository or a GitHub tree URL with
`voidc new install --git URL`. The compiler verifies and records the exact
commit and selected directory in the user template store. Install and
uninstall use a store separate from the library registrations under
`voidc package`.

Use a listed short name and `--name NAME` to generate a custom project.
The name selects the destination directory in the current directory. Use
`voidc new -- NAME` or `voidc new ./NAME` to create a built-in application
in a literal directory that matches an installed short name or a command
word.

An authored template contains a regular source project and declarative
metadata at `.void/template.toml`. See the author contract and a ready
to run example in [Custom VOID project templates](projects/G62_CUSTOM_PROJECT_TEMPLATES.md).

## Source package discovery and installation

```sh
voidc package list
voidc package search raylib
voidc package install mylib --git https://github.com/OWNER/REPOSITORY.git
voidc package install mylib --dir ../mylib
voidc add mylib
voidc add raylib
voidc add mylib --git https://github.com/OWNER/REPOSITORY.git
voidc add mylib --git https://github.com/OWNER/REPOSITORY/tree/main/packages/mylib
voidc add mylib --dir ../mylib
voidc tidy --check
voidc remove mylib
```

Discovery lists verified user registrations and the definitions shipped with
the compiler. It needs no network or account. `--dir` accepts the same paths
as `--path`. Give `package install` one source per alias.
An explicit HTTPS URL authorizes source acquisition for that URL. Without
`--rev`, the first addition pins the repository's HEAD in both
`void.project.toml` and `void.lock`. A repeat of the repository URL
preserves the existing pin. For a GitHub tree URL, VOID checks the
matching branch and tag names, including slashes, or the full commit
ID, then pins the selected commit and directory. An ambiguous tree
URL must be rewritten with its full commit ID. An alias already
locked to a different pin must be explicitly removed before adding
the new one. The selected Git package view is installed at
`libs/<alias>`. Advanced `--rev` and `--subdir` remain available
with repository URLs. `file:` Git origins require an explicit `--rev`.

The compiler creates the user package store on first installation:

- Windows uses `%LOCALAPPDATA%\VOID\packages`.
- Linux uses `$XDG_DATA_HOME/void/packages`, with
  `~/.local/share/void/packages` as the default.
- macOS uses `~/Library/Application Support/VOID/packages`.

Repeat installation from the same specified source verifies the existing
registration. To select different source bytes or a new Git pin, first
run `voidc package uninstall ALIAS`. `voidc add ALIAS` copies the verified
source and any prepared native catalog into the project. Git dependencies
keep their original URL, commit and subtree in the project's own records.
Projects continue to verify their own lock and require `--allow-native`
for native execution.

`voidc package uninstall ALIAS` removes the user registration.
`voidc remove ALIAS` removes a project dependency and its verified
managed view. Explicit editable local source is retained. Projects
already using a registered package keep their own copies.
Use `voidc new` to list starters or create a project. Use
`voidc new -- lib` when `lib` is a literal destination and you want
the legacy application starter.

Use `voidc package --project list` if an application directory is named
`list`, `search` or `install`. For the raylib program, prerequisites and
authorization commands, see [Bundled raylib](NATIVE_LIBRARIES.md#bundled-raylib).

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

## Source packages: add, remove and tidy

```sh
voidc add ALIAS [--project DIR] --dir DIR
voidc add ALIAS [--project DIR] --git URL
voidc remove ALIAS [--project DIR]
voidc tidy [--project DIR] [--check]
```

These commands default to the current project. `--dir` and its compatible
`--path` spelling are relative to that project. Ordinary local source already
inside the project remains editable in place. External source and declarative
native packages are installed under `libs/ALIAS`. The compiler records complete
lowercase immutable Git pins using D3's existing transport.
There is no bare-name registry lookup. `add` selects an application or library
manifest, validates the source library's package ID/version/entry, appends its dependency table
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
and [G26 qualification](projects/G26_PACKAGE_MAINTENANCE.md). Generated libraries
include their known `package.entry`; `init` does not infer one. For complete
application and library-consumer examples, see [creating and composing libraries](LOCAL_LIBRARIES.md#creating-and-composing-libraries)
and [G27 qualification](projects/G27_LIBRARY_AUTHORING.md).
