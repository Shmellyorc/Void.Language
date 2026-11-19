# G75 — CLI experience, build timings and terminal presentation

Status: **APPROVED FOR IMPLEMENTATION, 18 November 2026.**

Boss approved the five bounded stages in section 12 and the invocation-specific
F5 receipt guard, superseding the architecture-only hold. The review below is
retained as baseline evidence; its proposal language describes the reviewed
state. Current behavior, failures, performance and native-platform limits are in
the [implementation report](G75_CLI_EXPERIENCE_IMPLEMENTATION.md). Staging,
committing, pushing, installation and publication still require separate approval.

Reviewed 18 November 2026. Source baseline, checkout HEAD and local
`origin/main` are `fdcbeadbdd8a0011751b448afa9fe40e5151ab1f` (G74).
The remote-tracking reference was inspected without fetching; this is not a
new assertion about the live GitHub branch. G73 and G74 remain complete.
G69 file-icon work remains **ON HOLD**.

## 1. Recommended decision and scope

Use a small, dependency-free, line-oriented presentation layer. Keep the
existing parser and execution pipeline authoritative. Generate help columns
from command metadata, measure real scopes with a monotonic clock, and keep
timings separate from diagnostics and application output. Add neither FTXUI
nor Indicators. Do not introduce a full-screen UI, animation thread, terminal
input mode, new approval prompt, alternate compiler path or LLVM pass change.

Automatic concise totals should apply to interactive check/build/emit/package
and the compilation part of run. Preserve existing redirected stdout **and
stderr** by default. Offer explicit plain status for CI and an explicit file
for JSON timing reports. This resolves the existing exact-output contracts;
it requires Boss acceptance that automatic totals are terminal-only. Enabling
new output by default in every pipe is incompatible with those contracts.

This document proposes interfaces; none exists merely because it appears
below. Boss decisions are collected in section 13. The implementation stages
are bounded in section 12. This review adds only this document; it does not
authorize staging, commits, pushes, CI dispatch, installation or publication.

## 2. Current architecture and source evidence

Links and line numbers refer to the reviewed baseline. The
[Golden Wrench instructions](../WRENCH_CLANKER.md) govern research and
qualification; the [CLI reference](../CLI.md) describes existing contracts.

| Concern | Actual implementation and implications |
| --- | --- |
| CLI inventory, usage, help | [src/main.cpp](../../src/main.cpp#L29), `known_command`, `usage`, `help`, lines 29–159. Handwritten strings and spacing; no shared command registry. |
| Dispatch and parsing | [src/main.cpp](../../src/main.cpp#L718), `run_cli`. Creation dispatch runs first; metadata/help precede stdlib setup; specialized command parsers return before the common check/run/build/emit/package parser. This is not LLVM command-line parsing. |
| Creation and package maintenance | Same file: `create_project` 608, `custom_template_command` 563, `create_selected_template` 528, `maintain_project` 214, `discover_packages` 175; implementations in [project_creation.cpp](../../src/frontend/project_creation.cpp), [project_templates.cpp](../../src/frontend/project_templates.cpp), [project_resolution.cpp](../../src/frontend/project_resolution.cpp). |
| Dependency inspection/acquisition | `resolve_project` 302, `explain_project` 411, `fetch_project` 445 in main.cpp; graph, lock and Git work in project_resolution.cpp and [project_git.cpp](../../src/frontend/project_git.cpp). Ordinary compilation verifies local locks; it does not implicitly fetch Git sources. |
| Diagnostics | [source.cpp](../../src/frontend/source.cpp#L67), `render_diagnostic`, `CompilationError`; [diagnostic.hpp](../../include/void/diagnostic.hpp). Owned source spans, stable codes, multiline excerpts, hints and related locations. CLI warnings go to stderr; exceptions are printed there. |
| Frontend | [compiler.cpp](../../src/frontend/compiler.cpp): `lex` 240; `Parser` constructor 1389; `SourceGraph::parse_resolved` 6128; `validate_graph` 6357; `AnalysisAccess::build` 7170; `ValidatedAnalysis::lower` 7659; `read_source` 7694. Loading, imports and parsing are interleaved. |
| Manifest selection | [project_selection.inc](../../src/frontend/project_selection.inc#L21): `ProjectCapture`, `analyze_manifest_project_impl` 487, capture/recapture and native approval callback. Discovery parses candidate files, verifies locked inputs and checks that observations did not change. |
| Effective build configuration | [project.cpp](../../src/frontend/project.cpp#L788), `resolve_profile`; CLI main.cpp 917–927. Application profile, effective optimization and compiler's own CMake configuration are different facts. |
| LLVM/JIT/AOT | [backend.cpp](../../src/backend/llvm/backend.cpp#L135): `finish` 135, `write_object` 213, `Jit::Impl::load` around 3300, `Jit::compile` 3463, `compile_analysis` 3575, `emit_impl` 3704. LLVM optimization and object emission have real call boundaries; ORC materialization combines work at lookup. |
| Native object/link/publication | [native_build.cpp](../../src/native_build.cpp#L217): `execute` 217, `publish_object` 244, `link_executable` 251, `build_executable` 395, `build_application_package` later in the file. Link driver runs synchronously with inherited diagnostics; successful publication follows linking and verification. |
| Cache | [native_cache.cpp](../../src/backend/llvm/native_cache.cpp#L38) plus backend.cpp `compile_analysis`. Cache identity creation/verification and loading still cost time on hits. Windows persistence deliberately remains unavailable; do not infer a miss or promise a hit. |
| Native authorization | [native_approval.cpp](../../src/native_approval.cpp#L131), `interactive`, `confirm`, `authorize`, `remember`; main.cpp's `--allow-native` and `--reuse-native-approval`. Presentation must not decide consent, alter identity or read the prompt's input. |
| Existing console facilities | [console.cpp](../../src/runtime/console.cpp#L180), [console_presentation.inc](../../src/runtime/console_presentation.inc), [console_terminfo.inc](../../src/runtime/console_terminfo.inc), [console.void](../../stdlib/console.void#L204). Independent endpoint classification/capabilities, native Windows operations, optional POSIX terminfo, UTF-8/wide conversion. These belong to the Runtime, not an existing compiler status service. |
| VOIDUP | [cli.void](../../apps/voidup/cli.void), [main.void](../../apps/voidup/main.void), [manager.void](../../apps/voidup/manager.void), [transport.void](../../apps/voidup/transport.void), [inventory.void](../../apps/voidup/inventory.void), storage/trust modules beside them. This application is written in VOID, not a second C++ CLI. |
| VS Code | [extension.ts](../../editors/vscode/src/extension.ts#L150), [projectTask.ts](../../editors/vscode/src/projectTask.ts), [package.json](../../editors/vscode/package.json#L134), generated launch/task text in project_creation.cpp around 1125. `CustomExecution` spawns directly with pipes and literal argv, converts LF to CRLF, forwards both output streams, then closes with a process status. |
| Existing measurement tools | [compiler_project.py](../../tools/bench/compiler_project.py), [compiler_phases.cpp](../../tools/bench/compiler_phases.cpp), [measure_discovery.cpp](../../tools/measure_discovery.cpp), [G08 measurement record](../compiler-health/G08_COMPILER_PERFORMANCE.md). Reuse their fixtures, monotonic measurements and output oracles; do not create a competing benchmark suite. |

No general compiler progress/color policy or CLI timing switch was found in
these paths. Compiler diagnostics and help are plain text. Native approval
uses stdin plus stderr terminal checks (and POSIX foreground process-group
checks); `CI` explicitly blocks that durable-approval path. Its existing
interactive checks are security policy, not reusable status eligibility.

Runtime console support is more extensive: POSIX `isatty`, dimensions and
terminfo capability checks; Windows `GetConsoleMode`/screen buffer APIs;
UTF-8 text and native wide handling. Successful compilation must never
depend on these facilities. Windows `wmain` converts wide argv to UTF-8
before `run_cli`; preserve that and native process argument quoting.

## 3. Complete observed command and flag inventory

### Dispatch and help grammar

There are fourteen names in `known_command`: `check`, `run`, `build`, `emit`,
`init`, `new`, `add`, `remove`, `tidy`, `resolve`, `deps`, `package`, `native`,
`native-bind`. There is also the separately dispatched `help` command.
No short aliases for these fourteen top-level names were found.

Current source accepts these help/metadata forms:

| Form | Current behavior |
| --- | --- |
| `voidc`, `voidc -h`, `voidc --help`, `voidc help` | Same top-level help, stdout, status 0. |
| `voidc COMMAND -h`, `voidc COMMAND --help`, `voidc help COMMAND` | Help for each known top-level command, stdout, status 0, with exact argument counts. Many pages contain only usage plus a generic footer. |
| `voidc --version` | Compiler/development version, LLVM, target and build identity; stdout, status 0, no stdlib needed. |
| `voidc deps explain -h/--help`, `voidc deps fetch -h/--help` | Parent `deps` help. |
| `voidc native import --help` | Parent `native` help. `-h` is not a recognized nested help form here. |
| `voidc package install --help`, `voidc package uninstall --help` | Parent `package` help. No equivalent nested `-h` branch. |
| `voidc new install -h/--help`, `voidc new uninstall -h/--help` | Parent `new` help. |
| `voidc --emit-proof OUTPUT`, `voidc --jit-proof` | Internal architecture-proof modes, not ordinary global options. Preserve exact dispatch and existing result contracts. The JIT proof prints its result and succeeds only for 42. |

There is no universal help-token scan: `native approvals --help`,
`package list --help`, `new app --help` and `help native import` do not
currently select nested help. `native revoke --help` is parsed as a revoke
operand. Do not invoke it as a help probe. `--app` remains a removed option
with an early usage error, except in the literal forwarded application region.
Do not restore it. `--version` is also a creation-version operand after
`init`/`new`; it is not globally intercepted there.

The top-level screen currently prints all usage lines before the command
list. Its Options section advertises command-scoped flags as if global.
In fact `voidc --profile debug build` is not the supported placement:
the command is argv[1]. Preserve existing option/operand ordering **inside
each parser**, including negative legacy inputs and both meanings of `--`.

### Common source/build parser

Applies to check/run/build/emit and package assembly after specialized
dispatch. [main.cpp 826–958](../../src/main.cpp#L826) is authoritative.

| Existing argument | Scope, default and compatibility detail |
| --- | --- |
| File/directory selector | check and emit require one unless `--project` is given; run/build default to cwd. A directory selects its exact manifest if present, otherwise main.void. Explicit bare files keep their parent source root. No parent-manifest search or invalid-project fallback. |
| `--project DIR` | Select exactly DIR/void.project.toml; conflicts with positional selector. Also separately supported by maintenance/dependency parsers. |
| `--entry PATH` | Manifest-relative explicit entry; requires project mode. Library entry overrides are rejected. |
| `--profile debug\|release` | Project-only. Default release, except implicit debug selection by `--cache`. Manifest profile settings apply before `--no-opt`; this is not the compiler binary's CMake build type. |
| `--no-opt` | Disable O2; accepted by check without changing checking. Does not change the selected profile name. |
| `--cache` | Run-only; selects implicit debug and reports cache status to stderr. Requires effective Debug optimization none; conflicts with `--no-cache` and incompatible explicit profiles. |
| `--no-cache` | Run-only; disable disk persistence, not O2. Default run attempts eligible automatic caching even without `--cache`; optimized project cache use also exists. |
| `-o PATH` | build/emit/package only. Emit requires it. Build defaults to `<project-root or entry-parent>/build/<entry-stem>[.exe]`; assembly defaults to `<project>/dist`. Missing parents for native outputs are created after collision checks. |
| `--allow-native` | Invocation-only existing authorization in manifest execution/build/emission/package paths; rejected for check and non-project mode. Preserve all provider verification. |
| `--reuse-native-approval` | Internal editor contract: manifest run/build only, without `--allow-native`; reuse exact saved approval without prompting or saving. Do not redefine as a cosmetic flag. |
| `--replace` | Package assembly only. Linux can replace a verified same-application package; Windows still requires a fresh destination. |
| `--` | End options before a dash-leading source; in run after explicit source/project selection it instead begins literal application arguments. Do not consume any new flags after forwarding starts. |
| Second run operand | Legacy signed decimal i32 input, default 0. For actual applications use literal arguments after `--`; do not reinterpret their `--help`, `--timings`, quotes, spaces or Unicode. |

Duplicate options, unknown flags, missing values and cross-command conflicts
have explicit usage diagnostics. The parsers are not uniform about `--`,
dash-leading values or aliases. A help refactor must not silently normalize
their acceptance rules.

### Specialized commands and required detailed-help content

Each row is both an inventory and the content specification for its help
page. The common exit convention is 0 success, 1 compilation/operational
failure, 2 `UsageError`/native approval requirement. Some deeper validation
throws ordinary exceptions and therefore returns 1; do not rewrite those
as usage status 2. Run has the special contract described below.

| Command / accepted nested operations | Real arguments, defaults, effects and example for detailed help |
| --- | --- |
| `check` | Source or `--project`, optional entry/profile/no-opt. Analyze imports without startup execution; libraries support checking. Example `voidc check --project .`. Success stdout distinguishes source and library projects. |
| `run` | Common selector/options, optional legacy i32, literal application args. JIT compilation then invocation. Example `voidc run main.void -- --help "two words"`. Application status propagates; legacy result is printed and CLI exits 0 even when that result is nonzero. Native review may save exact consent only through the existing ordinary terminal path. |
| `build` | Common selector/options and optional `-o`; synchronous native object/link/publication. Example `voidc build --project . --profile debug`. Matching Runtime, verified resources, original Clang/LLD 23.1.4 SDK and host development libraries remain required. Native debug support is target-dependent. |
| `emit` | Explicit selector/project and required `-o`; same profile/no-opt/authorization admission. Example `voidc emit main.void -o main.o`. Host-native object, not an executable; caller must manually supply matching Runtime and dependencies to link it. |
| `init` | `[DIR]`, defaults cwd; `--kind application\|library`, `--name`, `--version`, `--project-id`, `--app-id`, `--package-id`, `--package-name`, `--nested`, source-path separator `--`. Writes a new manifest in an admitted existing directory. Default application, basename, 0.1.0, independently generated local IDs. Identity flags must match kind. Example `voidc init . --name Orbit`. |
| `new` | No operands or `list` lists templates; `list --json` is schema 1. Built-ins `app`, `classlib`, alias `lib`; installed short names; ordinary PATH creation; creation flags above plus `--source`. Example `voidc new app --name Orbit`. `.` requests empty in-place built-in creation; `./PATH` or `-- PATH` preserves literal starter-looking paths. New paths publish exclusively; nested manifests need `--nested`. Default app source src/app.void; library source src/<package-name>.void. |
| `new --template` | Exact `SHORT-NAME --name NAME [--in-place]`; schema-1 root JSON on stdout, used by editor creation. In-place applies to built-ins only. Example `voidc new --template app --name Orbit`. No flags may be casually reordered in this exact-count grammar. |
| `new install`, `new uninstall` | Install exactly `--dir PATH` or `--git URL`; uninstall SHORT-NAME. Example `voidc new install --dir ./templates/game`. Installed templates produce files; their source is not a native-execution approval. Built-ins cannot be uninstalled. |
| `add` | ALIAS, optional `--project` (default .), `--dir` / accepted alias `--path`, `--copy`, `--git ORIGIN`, `--rev FULL_SHA`, `--subdir`; also `--bundled`, `--registered IDENTITY`, `--edit-token TOKEN`, `--suggest-alias --dir DIR`. Example `voidc add geometry --dir ../geometry`. Alias-only selects registered/bundled package; verified bundled raylib has target/resource restrictions. Suggest-alias emits schema-1 JSON with nullable alias. Edit-token/registered identity protect editor edits. Git admission and content/lock checks remain mandatory; missing revision follows existing snapshot resolution, not an invented default branch promise. Native package preparation does not grant execution consent. |
| `remove` | One ALIAS, optional `--project` (default .), `--edit-token`. Example `voidc remove geometry --project .`. Removes a direct dependency and reconciles installed views, not native approvals. |
| `tidy` | `--project` (default .), optional `--check`. Example `voidc tidy --project . --check`. Validate imports and reconcile dependencies/views; check mode tests consistency. Interrupted edit/recovery failures remain errors, not a new automatic recovery UI. |
| `resolve` | Requires `--project DIR`, optionally one of `--locked`, `--refresh`. Example `voidc resolve --project . --locked`. Default create/verify available local lock; refresh explicitly replaces stale lock; locked requires existing lock when dependencies require it. No implicit Git fetching. |
| `deps explain` | Requires `--project`; `--json`; internal `--for-edit` requires JSON. Example `voidc deps explain --project . --json`. Verified local graph; no acquisition/publication. Schema 1, deterministic ordering, 4 MiB bound; operational JSON error is stdout object with status error and exit 1. Malformed invocation remains stderr/2. |
| `deps fetch` | Requires `--project`, repeated `--allow-git ORIGIN REV` pairs subject to existing limits and immutable pin validation. Example `voidc deps fetch --project .`. Acquires declared pinned Git snapshots; explicit transitive-origin/revision authorization is preserved. |
| `package` assembly / `--verify DIR` | Assembly needs manifest application and supports common flags, output, profile, allow-native, replace. Example `voidc package --project . --profile release -o dist`. Linux x86-64 GNU and Windows x64 MSVC assembly only; verification is exact `voidc package --verify dist`, does not build. Integrity checks and relocation/publication rules remain unchanged. |
| `package list`, `package search NAME` | List, or exact list `--json` (schema 1), or one non-dash search name. Example `voidc package list --json`. Search is installed/bundled alias substring discovery, not an online package registry query. Availability is compiler/target dependent. |
| `package install`, `package uninstall` | Register ALIAS with `--dir`/`--path` or `--git`, optional rev/subdir; uninstall one alias. Example `voidc package install geometry --dir ../geometry`. User package registry; no `--project`, `--copy`, `--edit-token` or execution approval. Uses the maintenance parser's registration mode, not application packaging. |
| `native import` | Header plus required `--provider ID --out NEW-DIR`; optional `--source` (default /native.void), `--clang` (installed configured driver), `--language c\|c++` (default c); preprocessing tokens after `--`, with existing validation. Example `voidc native import api.h --provider demo --out bindings -- -Iinclude`. Runs header inspection and publishes editable candidates; does not authorize provider execution. Respect existing Clang/version/input bounds and exclusive output policy. |
| `native approvals`, `native revoke` | List exact saved records or revoke exact APPROVAL_ID. Example `voidc native approvals`. Revoke mutates the existing store only through its controlled path. Malformed ID is an approval requirement (2); missing record is operational failure (1). Never probe revoke with a help-looking operand. |
| `native-bind` | Exact `DESCRIPTOR C-HEADER -o NEW-OUTPUT`; accepted output suffixes .c **and .void**, despite usage showing only adapter.c. Example `voidc native-bind provider.toml api.h -o adapter.c`. .c generates checked C11 adapter; .void generates declarations. Exclusive creation, bounded descriptor admission, no consent or ABI change. |
| `help` | `help [COMMAND]`, existing aliases `COMMAND -h/--help`, plus top-level -h/--help. Example `voidc help native`. Help is metadata, must remain usable without installed resources. |

Native-import defaults are declared in
[native_import.hpp](../../include/void/native_import.hpp); source selection and
creation details are in [CLI.md](../CLI.md) and the linked implementations.
Detailed help must describe a relevant exception rather than attach the
current generic “run/build default to cwd” footer to every unrelated command.

## 4. Proposed help design

Top-level help retains the compiler/LLVM identity lines and documentation
link, has one usage synopsis and a dynamically aligned command table. The
same renderer serves no args, -h, --help and help. Example layout:

```text
VOID Language Compiler v0.1.0 (development)
LLVM 23.1.4

Usage: voidc <command> [arguments] [options]

Commands:
  check        Check source and imports.
  run          Compile and run through the JIT.
  build        Build a native executable.
  emit         Write a native object.
  init         Add a project manifest.
  new          Create a project or manage templates.
  add          Add a source dependency.
  remove       Remove a direct dependency.
  tidy         Check or reconcile dependencies.
  resolve      Verify or create a dependency lock.
  deps         Explain or fetch dependencies.
  package      Assemble an app or manage source packages.
  native       Import headers or manage native approvals.
  native-bind  Generate an adapter or native declarations.
  help         Show command help.

Options:
  -h, --help   Show help.
  --version    Show compiler, LLVM, target and build identity.

Use 'voidc <command> --help' for arguments, options and examples.
Documentation: https://github.com/Shmellyorc/Void.Language/wiki
```

Calculate `width = max(display_width(name))` over the displayed command
records; emit two leading spaces, name, `width - display_width(name) + 2`
spaces, description. Current names are ASCII, so byte length is sufficient
for them; do not confuse arbitrary UTF-8 byte counts with display widths.
Apply the same algorithm independently to option and nested-command tables.
Continuation lines begin at the description column. Use **spaces, no tabs**.
Wrap description paragraphs only; narrow terminals may put descriptions on
the next indented line. Never truncate a command or meaningful operand.
Redirected help uses a deterministic width independent of terminal probing.

Use one command metadata list for inventory/help, with explicit bindings to
the existing dispatchers. Do not replace these parsers with a new CLI library
as part of G75. Retain `known_command`'s distinction if help is still separately
dispatched. Moving existing common options to their actual command pages is
an intentional help-text change, not removal of those flags. The old
top-level tests requiring `--no-opt`, `--cache`, `--no-cache`, `-o PATH` in
the top Options screen must be updated explicitly to check command pages.

Every command page uses: identity; focused usage; short effect description;
arguments/defaults; applicable options/conflicts; nested operations; examples;
security or write effects; exits. Section 3 supplies the page-specific content.
For example, the build page should read approximately:

```text
Usage: voidc build [file-or-directory] [options]
       voidc build --project DIR [options]

Compile, link, verify and publish a native executable.
Default source: current directory. Default output: build/<entry-stem>[.exe]
under the selected project root or standalone entry directory.

Options:
  -o PATH               Set the executable path.
  --project DIR         Select exactly DIR/void.project.toml.
  --entry PATH          Select a manifest-relative entry.
  --profile debug|release
                        Select a project profile; default release.
  --no-opt              Disable O2 without changing the profile name.
  --allow-native        Use the existing invocation-only native authorization.
  --reuse-native-approval
                        Reuse exact saved native approval without prompting.
  -h, --help            Show this help.

Requires the matching Runtime, Clang/LLD 23.1.4 SDK and host libraries.
Native execution retains all existing verification and consent rules.
Examples:
  voidc build .
  voidc build --project . --profile debug -o "output folder/hello"
Exit: 0 success; 1 compilation/toolchain/publication failure;
      2 invalid invocation or native approval required.
```

After approved timing/status implementation, append those actual flags to
applicable pages. Do not advertise them beforehand.

Provide focused pages for the **already accepted** nested help forms, including
native import and package install. Other nested forms that currently fail
remain unchanged unless Boss explicitly approves extending the grammar.
In particular, a generic “find help anywhere” pass could steal literal paths,
creation version values and forwarded app options and is rejected.

## 5. Output contracts and proposed status behavior

### Existing streams and exits to preserve

| Channel / operation | Existing contract |
| --- | --- |
| Help/version | stdout, no stderr; help equality aliases; first two identity lines tested. |
| Successful check/build/emit/package | stdout records such as `Source checked successfully`, `Built native executable: PATH`, `Wrote native source object: PATH`, `Packaged application: PATH`. Preserve them verbatim rather than move them to stderr. |
| Application run | Application stdout/stderr belong to the application. Modern application return status propagates; legacy entry prints its integer and returns CLI status 0. Native status truncation remains platform behavior. |
| Compiler errors and warnings | stderr, structured diagnostic text; usage errors also print usage and help guidance. `CompilationError` text already contains rendered diagnostics: never render that vector a second time. |
| Explicit `--cache` | `Debug cache: STATUS` on stderr; tests and benchmarks compare this exactly. |
| Machine interfaces | deps explain/list/template JSON and add suggest-alias JSON are stdout schemas, not places for status. new --template emits schema-1 root JSON. Operational deps explain errors use its JSON stdout path; do not generalize that behavior to other commands. |
| Linker children | `ExecuteAndWait` inherits stdout/stderr for linking. Version checks capture stdout privately. There is no safe basis to assume every external tool writes diagnostics only to stderr. |
| VOIDUP | Data/status lines from `up_say` are stdout; errors are structured JSON on stderr; forwarded tools inherit their output and status. Error consumers parse stderr as JSON. |
| LSP | Content-Length framed stdout from voidls is a protocol, not a human status sink. G75 must not add compiler observer output to it. |

Default CLI status 0/1/2 must remain unchanged. Native signal exits are not
newly mapped by presentation; VS Code and VOIDUP already map signaled child
processes using 128+signal. The compiler has no new cancellation protocol
to promise. A native linker nonzero is currently a CLI failure 1, not the
linker's raw status.

### Recommended additive policy

Proposed command-scoped `--status auto|plain|off` on check/run/build/emit/package:

| Mode / destination | Proposed behavior |
| --- | --- |
| Default auto, stderr attached to terminal, outside CI | Concise newline status on stderr plus an automatic total. No color or cursor control required. |
| Default auto, stderr redirected/piped, CI present, or capability detection fails | No additional status. Preserve baseline bytes on both streams. A redirected stdout alone does not prove stderr is non-terminal. |
| Explicit plain | Same newline status and total on stderr, including CI/pipes. Never ANSI, CR redraw, spinner, percentage or terminal-width dependence. This is an explicit output-contract opt-in. |
| Explicit off | Suppress new presentation only. Do not suppress existing diagnostics, cache status or stdout success/data lines. |
| Existing JSON commands / voidls / proof modes / forwarded application args | No status option injection, observer output or terminal control. Reject unsupported new compiler flags through existing usage handling. |

Example status lines below are **illustrative**, not measured G75 results.
Each is emitted only at its real boundary:

```text
Checking   project sources
Compiling  project
Linking    executable
Finished   Release build in 0.84s
Running    JIT application
```

`Checking` begins source analysis; `Compiling` begins lowering/backend work;
`Linking` begins the actual AOT driver invocation, not merely an anticipated
step. JIT does not have an external executable path: never print
`Running ./build/app` for `voidc run`. Emit has no linking step. A cache hit
does not get a fabricated Compiling or O2 phase. Package assembly can add
`Packaging` only around real inventory/staging/publication work.

Use effective manifest profile name for Debug/Release, separately showing
effective optimization where useful. A release profile plus --no-opt remains
Release with O2 disabled. Bare-file builds have no manifest profile: say
`Finished build ...` and record `profile: null`. Never infer the application
profile from CMake Debug/Release. `check` says “check”, not “build”; emit says
“object emission”; package says “package”. Retain target limitations from
native_source_debug_supported and application packaging.

Stop compilation timing before application invocation. A run can have a
successful compilation followed by an application exit 7: show the two facts
separately. Do not call the run successful merely because JIT compilation
succeeded. Detailed timing may show `Application wall time ... (exit 7)` after
invoke; ordinary status need not add a runtime footer to every application.

Report `Failed build after ...` only after the existing error diagnostic has
been emitted, without a Finished line. Failure total includes work to the
failure point. For ordinary caught failures use explicit outcome state, not
“destructor ran, therefore success”. On interrupt, no success is emitted;
if control does not return normally, a footer/report may be absent. SIGKILL,
crashes and forced termination cannot guarantee cleanup or a timing file.
Do not add signal handlers or alter cancellation semantics for presentation.

Keep output at command/major-phase granularity, not per file/token/pass.
Never sleep to expose a phase, estimate percent completion, invent ETA,
buffer/replay diagnostics, or asynchronously interleave status with them.
Serialize compiler-owned status at existing call boundaries and finish each
record before diagnostics. Cross-stream global ordering with inherited
children is not guaranteed today; do not claim it is. No animation during
linker execution, native approval prompts or application execution.

**Spinner decision: defer.** Newline status meets the need without a renderer,
worker or cleanup problem. If later justified, require independent stderr
capabilities, explicit opt-in, delayed first frame, a bounded refresh rate,
exclusive output ownership and cleanup before any diagnostic/prompt/child.
An `isatty` result alone does not establish ANSI or Unicode support. Screen
readers and reduced-motion users retain the complete plain path. Future color
must honor NO_COLOR and reset only its own attributes; the initial design
requires no color at all. Escape control bytes in newly formatted untrusted
labels/paths using the existing native-approval display approach; do not
change canonical identities or existing diagnostic text as a side effect.

## 6. Real timing scopes and limitations

### Total duration

Use `std::chrono::steady_clock` in the C++ compiler, as the existing phase
probe does. For an applicable invocation, start before stdlib configuration
and normal command work in run_cli, after metadata-only dispatch has been
excluded; end after successful check/result or output verification/publication,
or at a caught failure. Include parsing/selection/verification work in that
defined interval. It excludes process-loader startup, final rendering/report
serialization and process teardown. Name it **build wall time**, not CPU time
or whole-process time. External benchmark timers remain authoritative for
whole-process latency.

For run, end build wall time after JIT compile and argument preparation, before
`code->invoke`. Measure invocation-to-return as a separate application wall
interval; do not include program runtime in a compiler total. AOT build does
not execute the output and has no application runtime measurement. Package
total includes its real assembly verification/publication, not just linking.

Native authorization can wait for a human inside analysis. Keep inclusive
build wall time honest and label this inclusion when it occurs; optionally
record an `authorization_wait` child around the existing callback. Do not
claim it is compiler CPU or silently subtract it from the outer wall interval.
Refused authorization remains its existing outcome/status. No prompt/UI
behavior is changed by adding a scope.

A common outer interval is reliable across JIT/AOT and supported project
profiles, but its work differs by command, cache state and platform. It is
not a basis for claiming all modes have comparable phase sums.

### Candidate instrumentation map

These are proposed observation points, not existing public timing APIs.
Attach a nullable invocation-owned sink with fixed phase IDs, not a global
ostream or state in ValidatedAnalysis. LSP/library users default to no sink.
Capture scopes around current operations; do not restructure compilation to
make timing easier.

| Phase ID | Exact proposed boundary | Availability / nesting |
| --- | --- | --- |
| `frontend` | analyze_manifest_project[_with_native_approval] or analyze_project call in run_cli | Inclusive parent for capture, loading, parsing, semantics, analysis assembly, revalidation and possible authorization wait. |
| `project_capture` | ProjectCapture construction, inventory/selection and fresh recapture blocks in project_selection.inc | Several scopes, not one contiguous discovery stage. Repeated verification stays measured; never remove it. Manifest parsing distinct from source parsing. |
| `dependency_verification` | verify_locked_dependencies in ProjectCapture, including initial/fresh capture | Local lock/source/provider validation, nested in project capture. Acquisition belongs only to deps fetch; do not label verification a network resolve. A separate future resolve/deps timing feature can wrap their existing operations. |
| `source_loading` | read_source and ProjectCapture::read_bounded, including SourceFile construction | Includes I/O, UTF-8 validation and snapshot construction; do not call it pure disk read time. Overlay/synthetic inputs and package snapshots need explicit source-kind accounting, or remain inside frontend. Avoid double-counting wrappers. |
| `lex_parse` | Parser construction **and** parse call at SourceGraph::parse_resolved and equivalent source entry sites | Constructor calls lex, including recursive interpolation lexing. Start before construction; do not time only parse and label it lexing. Initial implementation combines lex/parse; a separate lex child would be nested, not additive. |
| `semantic_analysis` | validate_graph calls in compiler.cpp and project_selection.inc | Includes type/generic/constant analysis performed there; not the entirety of frontend. Repeated/current capture validation is part of actual execution, not a fictional single pass. |
| `analysis_assembly` | AnalysisAccess::build | Builds retained analysis, compatibility and editor metadata; keep distinct from validate_graph and deferred lowering. |
| `typed_ir` | ValidatedAnalysis::lower (including lower_graph and ir validation) | Actual call, with emission retained as used today. Absent on check and cache-hit paths; never lower again for measurements. |
| `llvm_ir` | backend lower plus actual AOT entry/application adapter/metadata construction before finish | JIT and AOT have different construction work. AOT lower alone omits later adapter construction; scope the complete construction region and say so. |
| `llvm_verification` | verifyModule calls in finish | Separate child scopes for pre/post verification. Do not disable checks or change exception order. |
| `llvm_optimization` | Existing optimize branch in finish: analysis managers, setup, existing scalar-store cleanup, O2 pipeline.run | Optional nested pipeline sub-scope. Includes setup as named; post-verification is separate. O0 is skipped, not measured as invented zero work. No changed passes, tuning or target policy. |
| `native_object_emission` | write_object in AOT | Pass-manager setup, code generation, object serialization and flush/error check combined. Do not claim a pure instruction-selection time; codegen/write cannot be split cheaply from this wrapper. |
| `jit_materialization` | Jit::Impl::load across addIRModule/addObjectFile, lookup and Runtime metadata admission | Combined ORC codegen/link/load/registration wall time. Do not split JIT codegen and JIT link into invented values. More internal attribution requires a separately justified ORC observation design. |
| `cache_lookup`, `cache_store` | Actual NativeCache read/validation/key work and write in compile_analysis | Cache hit/miss/unavailable/disabled recorded from actual state. A failed hit candidate can fall back to compilation; preserve both work and final cache status. |
| `toolchain_validation` | resources/version/inventory validation and associated staging in native_build.cpp | Version subprocesses are not the final linker. Include overhead in build wall time. |
| `native_link_driver` | Only execute(args) after emit_object at native_build.cpp 353 | Wall time for Clang driver + linker process + wait; not standalone LLD CPU time. Inherited diagnostics remain unchanged. |
| `publication`, `package_assembly` | Existing output verification/copy/publish and package inventory assembly/publish scopes | Success only after these finish. Filesystem failures after link are still failed builds. Parent/child intervals reflect actual containment. |
| `application` | code->invoke in main.cpp | Outside build, separate status/duration. May not return after a signal/exit/crash. No external debugger duration inference. |

Frontend analysis can return a failed AnalysisResult without throwing. Mark
phase success from succeeded() and existing result/error checks, not merely
from normal scope exit. The same applies to failed subprocess results.

Use fixed counters/aggregate storage to avoid unbounded per-file event logs.
Within one category compute the union of nested same-category intervals or
otherwise exclude recursion; a recursive lex call must not inflate totals.
If aggregating repeated disjoint scopes, label the record `aggregate` with a
count, no invented continuous start/end. Parent inclusive durations and
child aggregates must not be added together. Do not derive “other” by
subtracting overlapping categories; uncovered time may remain uncovered.
Future concurrent scopes require interval/thread metadata and explicit
overlap semantics before they can be combined.

Clock units are not clock accuracy. Record the clock's nominal tick period
and mark effective resolution unknown unless calibrated. Store integer
microseconds rounded once from the native duration; report sub-microsecond
results as below resolution rather than fake precision. Text totals can use
hundredths of a second and `<0.01s` for very short work; details can use ms/us
with honest resolution. Avoid locale-dependent decimal/JSON formatting.

## 7. Proposed structured timing interface

Current parsers have no --timings or --timings-format. Add them only to the
common applicable source/build grammar, before the run forwarding boundary:

```text
voidc build --project . --timings
voidc build --project . --timings --timings-format json --timings-output report.json
voidc run main.void --timings --timings-format json --timings-output run.json -- app-arg
```

Recommend `--timings-format text|json` (text default), with format requiring
--timings. `--timings-output PATH` requires JSON; JSON requires an explicit
output path. Reject duplicate/missing/unknown values as existing usage status
2. The tentative shorter `--timings --timings-format json` alone should be a
usage error pointing to --timings-output. Do not offer `-` as stdout/stderr:
build stdout already has success records, linker children inherit streams,
and run has arbitrary application output. Existing --json commands keep
their own schemas and reject timing flags until separately designed.

Text detail goes to stderr by explicit request, after the relevant build or
failure diagnostics, with application timing separate after invocation returns.
With JSON selected, suppress new human status/timing text; write one complete
UTF-8 JSON object to the selected file. Existing diagnostic/application streams
remain verbatim. JSON encoding escapes control characters; there are no raw
ANSI escapes, spinner sequences or diagnostic blobs in the timing document.

Resolve/reserve the timing destination before work, with exclusive creation
and existing protected-store/path admission facilities. Reject collisions
with source, manifest, lock, cache, native payload, executable/object/package,
installed stores or existing files; reject aliases/reparse/symlink substitution
through the existing controlled publisher. If a selected protected project
tree cannot be safely admitted before discovery, reject that timing location
and request a fresh report path outside it. Never overwrite an existing
report or weaken compiler output protections to save measurements.

Publish a complete report atomically after the final observed outcome.
On a caught build failure, publish partial measurements when possible without
replacing the diagnostic or exit. Abrupt termination can leave no report;
benchmark tooling records that externally. On report I/O failure, preserve the
compiler/application exit and output artifact; emit a distinct stderr timing
report error, never a fake successful report. Consumers must check file
existence and validity as well as process status. Boss must approve this
best-effort telemetry policy; it avoids turning a successful application's
status into a timing-system status.

### Version 1 data contract

This is a proposed schema, independent of existing schema-1 package/dependency
documents. Keys below are mandatory unless explicitly nullable; additions are
backward-compatible, semantic changes require a new schema_version. Consumers
reject unknown major versions, tolerate new fields and phase IDs, and never
interpret null as zero. Detailed paths/argv are omitted by default to avoid
recording application secrets. Compiler build identity is emitted as metadata,
not assumed equal to the Git commit.

| Field | Type and meaning |
| --- | --- |
| `schema`, `schema_version` | Fixed `void.cli.timings`, integer 1. |
| `tool`, `compiler_version`, `compiler_build_identity`, `llvm_version`, `target` | Strings; actual values from the executing compiler, never hardcoded examples. |
| `command`, `execution_mode` | Command string; check/aot/jit/package. |
| `configuration` | profile string debug/release or null; effective optimization none/O2 or null before resolution; debug_info none/line/full/unavailable or null; project_mode boolean or null. Record compiler build configuration separately only if known. |
| `cache` | `{status: hit|miss|disabled|unavailable|not_applicable|unknown}`. Not guessed from --cache presence. |
| `clock` | kind monotonic, unit us, nominal_tick_ns number, effective_resolution_us number or null. Nominal period is not a calibrated resolution. |
| `build` | outcome success/failure/approval_required/invocation_error/interrupted; wall_duration_us integer >=0 or null if unstarted; includes_authorization_wait boolean; complete boolean. Interrupted only if observed without new signal handling. |
| `application` | entry_contract application/legacy_i32 or null; state not_started/running/returned/interrupted; wall_duration_us integer or null; exit_code integer or null for a modern application; legacy_result integer or null for the legacy returned value. `build` success does not imply application status 0. |
| `process_exit_code` | Actual returned CLI code, including the legacy-entry exception; null if a report is finalized before a code is available. It is not the linker child status. |
| `phases` | Array with unique ID, parent_id or null, kind scope/aggregate/unavailable, state complete/partial/skipped/unavailable/not_reached, duration_us integer or null, count integer, reason string or null. Inclusive wall durations; aggregates contain disjoint invocations for that category, with recursion removed. |
| `measurement` | `{durations: inclusive_wall, phase_sum_is_total: false, detailed: boolean}`. No inferred CPU times. |

Phase IDs in section 6 are the initial vocabulary. Producers may omit
uninstrumented categories; consumers treat missing IDs as unavailable, never
as zero or proof that work was skipped. A producer that knows why a phase was
skipped/not reached should emit that explicit state. Parent IDs must reference
records in the same report and form an acyclic containment hierarchy.

No fabricated phase duration for O0, a hit, a failed frontend or an unsupported
platform: use skipped/unavailable/not_reached plus reason and null duration.
`partial` means elapsed work up to a caught failure, not a completed phase.
The report contains bounded aggregates, not per-token events. A phase lacking
a trustworthy boundary is unavailable with `reason: combined_boundary`.

Illustrative successful AOT report (values are invented **only to demonstrate
the proposed format**, not benchmark observations):

```json
{
  "schema": "void.cli.timings",
  "schema_version": 1,
  "tool": "voidc",
  "compiler_version": "0.1.0",
  "compiler_build_identity": "example-build-identity",
  "llvm_version": "23.1.4",
  "target": "x86_64-unknown-linux-gnu",
  "command": "build",
  "execution_mode": "aot",
  "configuration": {"profile": "debug", "optimization": "none", "debug_info": "full", "project_mode": true},
  "cache": {"status": "not_applicable"},
  "clock": {"kind": "monotonic", "unit": "us", "nominal_tick_ns": 1, "effective_resolution_us": null},
  "build": {"outcome": "success", "wall_duration_us": 840000, "includes_authorization_wait": false, "complete": true},
  "application": {"entry_contract": null, "state": "not_started", "wall_duration_us": null, "exit_code": null, "legacy_result": null},
  "process_exit_code": 0,
  "phases": [
    {"id": "frontend", "parent_id": null, "kind": "scope", "state": "complete", "duration_us": 240000, "count": 1, "reason": null},
    {"id": "lex_parse", "parent_id": "frontend", "kind": "aggregate", "state": "complete", "duration_us": 50000, "count": 3, "reason": null},
    {"id": "llvm_optimization", "parent_id": null, "kind": "unavailable", "state": "skipped", "duration_us": null, "count": 0, "reason": "optimization_disabled"},
    {"id": "native_link_driver", "parent_id": null, "kind": "scope", "state": "complete", "duration_us": 200000, "count": 1, "reason": null}
  ],
  "measurement": {"durations": "inclusive_wall", "phase_sum_is_total": false, "detailed": true}
}
```

Illustrative failed check report:

```json
{
  "schema": "void.cli.timings",
  "schema_version": 1,
  "tool": "voidc",
  "compiler_version": "0.1.0",
  "compiler_build_identity": "example-build-identity",
  "llvm_version": "23.1.4",
  "target": "x86_64-unknown-linux-gnu",
  "command": "check",
  "execution_mode": "check",
  "configuration": {"profile": null, "optimization": null, "debug_info": null, "project_mode": false},
  "cache": {"status": "not_applicable"},
  "clock": {"kind": "monotonic", "unit": "us", "nominal_tick_ns": 1, "effective_resolution_us": null},
  "build": {"outcome": "failure", "wall_duration_us": 13000, "includes_authorization_wait": false, "complete": false},
  "application": {"entry_contract": null, "state": "not_started", "wall_duration_us": null, "exit_code": null, "legacy_result": null},
  "process_exit_code": 1,
  "phases": [
    {"id": "frontend", "parent_id": null, "kind": "scope", "state": "partial", "duration_us": 12000, "count": 1, "reason": "source_error"},
    {"id": "llvm_ir", "parent_id": null, "kind": "unavailable", "state": "not_reached", "duration_us": null, "count": 0, "reason": "frontend_failed"}
  ],
  "measurement": {"durations": "inclusive_wall", "phase_sum_is_total": false, "detailed": true}
}
```

For a warm JIT hit, `cache.status` is hit; typed_ir/llvm_ir/optimization are
skipped with cache_hit, while real cache verification and jit_materialization
durations remain. A run returning 7 records build success, application returned
with entry_contract application, exit_code 7 and process_exit_code 7. A legacy
entry returning 7 records entry_contract legacy_i32, exit_code null,
legacy_result 7 and process_exit_code 0. These meanings must remain distinct.

## 8. Upstream research and terminal-library comparison

Research was performed before recommending implementation work, following
the Golden Wrench rule. The sources below are existing upstream implementations
or documented features, **not proposed or abandoned fixes**. No pending patch
is required or recommended. Moving upstream branches were read on 18 November
2026; any later dependency adoption must choose an immutable revision and
verify its contents. No library was downloaded into the checkout or added.

| Option | Evidence, integration cost and qualification limits |
| --- | --- |
| FTXUI | Upstream advertises no external dependencies, UTF-8/fullwidth support and Linux/macOS/Windows portability. Its screen/DOM/component architecture exceeds line status needs. Integration would add compiled screen/layout code and potentially component/event-loop machinery; no binary-size or build-time estimate was measured. [README](https://github.com/ArthurSonzogni/FTXUI), [CMake source](https://github.com/ArthurSonzogni/FTXUI/blob/main/CMakeLists.txt). |
| FTXUI build/terminal details | Current CMake declares 7.1.0; normal targets require C++17, separate C++20 modules are optional. Terminal source contains Windows ASCII/cursor quirks and platform-specific size/color logic; that is upstream support, not proof of correct behavior on VOID's stderr, pipes, Windows consoles or `/MT` packaging. A local adapter and native qualification would still be required. [Build options](https://github.com/ArthurSonzogni/FTXUI/blob/main/cmake/ftxui_set_options.cmake), [terminal source](https://github.com/ArthurSonzogni/FTXUI/blob/main/src/ftxui/screen/terminal.cpp). |
| Indicators | Header-only bars/spinners; smaller integration than FTXUI, but CMake 2.3.0 uses C++11 and Threads::Threads. Header-only does not mean zero compile/link footprint. It would remain C++-specific and need separate VOIDUP support. [README](https://github.com/p-ranav/indicators), [CMake](https://github.com/p-ranav/indicators/blob/master/CMakeLists.txt). |
| Indicators rendering details | Spinner defaults to Unicode frames and stdout, redraws using carriage return and flushes on update; supports a selected stream and uses a mutex. Its timer uses high_resolution_clock, which is not the proposed monotonic compiler contract. terminal_size/cursor helpers target stdout; cursor-control code can emit escapes directly. Termcolor detects stream TTY/color separately, but this is not a complete redirected-output/progress policy. VOID would still need its own gating, diagnostic serialization, ASCII fallback and cleanup. [Spinner source](https://github.com/p-ranav/indicators/blob/master/include/indicators/progress_spinner.hpp), [terminal size](https://github.com/p-ranav/indicators/blob/master/include/indicators/terminal_size.hpp), [cursor control](https://github.com/p-ranav/indicators/blob/master/include/indicators/cursor_control.hpp), [termcolor](https://github.com/p-ranav/indicators/blob/master/include/indicators/termcolor.hpp). |
| Current VOID facilities | Existing ostream/LLVM Support output plus a small endpoint check suffice for compiler newline status. VOIDUP can use console.stderr capabilities/write and time.monotonic_now/elapsed. No new package, Unicode renderer or event loop. Some duplicate host/language formatting is cheaper than a new shared ABI. Native Windows/POSIX checks still need focused qualification. |

FTXUI and Indicators do not establish a screen-reader/reduced-motion guarantee
for this integration. Both need plain equivalent output, non-color state cues
and animation opt-out. FTXUI's interactive components are unnecessary; neither
library prevents diagnostic interleaving or decides when a compiler phase is
complete. Indicators' mutex protects its own state, not all LLVM, linker and
application writes. POSIX terminal emulators, Windows Terminal/ConPTY, classic
Windows consoles and CI pipes are separate qualification cases.

Licensing/package implications: VOID is MIT. FTXUI's
[MIT license](https://github.com/ArthurSonzogni/FTXUI/blob/main/LICENSE) requires
retaining its notice. Indicators has an
[MIT license](https://github.com/p-ranav/indicators/blob/master/LICENSE) and
bundled termcolor's separate
[BSD-style terms](https://github.com/p-ranav/indicators/blob/master/LICENSE.termcolor),
including redistribution notices and non-endorsement conditions. Any adoption
would require notice/inventory updates, reviewed source pinning, offline build
support and testing static CRT/relocation on the actual release platforms.
Nothing here recommends a build-time network fetch. Dependency-free status
adds no third-party notice or package and has the smallest maintenance surface.

Other proven approaches checked:

- Cargo documents separate build timing reports and concurrency information;
  its timing graph is not a claim to measure all work inside rustc. Reuse the
  separation of build and execution, not Cargo-specific phases or defaults.
  [Cargo timing reports](https://doc.rust-lang.org/cargo/reference/timings.html).
  Its terminal policy has explicit auto/never controls; this supports making
  progress a presentation policy, not compilation authority.
  [Cargo terminal configuration](https://doc.rust-lang.org/cargo/reference/config.html#term).
- LLVM already provides nested RAII tracing and Chrome trace output. The
  pinned [23.1.4 TimeProfiler source](https://github.com/llvm/llvm-project/blob/llvmorg-23.1.4/llvm/include/llvm/Support/TimeProfiler.h)
  and the local SDK header expose initialization, scope, write and cleanup
  APIs. They require thread/lifecycle management and their trace schema is not
  a stable VOID benchmark schema. The source warns about string-building
  measurement effects. Use coarse invocation-local steady_clock scopes first;
  do not enable global tracing/pass instrumentation in normal CLI operation.
  LLVM's [opt timing option](https://llvm.org/docs/CommandGuide/opt.html)
  is another implemented tool feature, not a flag already accepted by voidc.
- Windows documents GetConsoleMode against a specific console handle; POSIX
  and Windows capability checks must examine the actual status endpoint.
  [Microsoft console API](https://learn.microsoft.com/en-us/windows/console/getconsolemode).
  VOID already uses this approach. No terminal-ownership library is needed.

**Decision: no new dependency is justified for G75.** An animation requirement
would need new evidence, not a speculative framework addition now.

## 9. VOIDUP and VS Code integration

### VOIDUP

VOIDUP currently accepts global `--root`, `--catalog`, `--ca-file`,
`--timeout-ms` **before** the command. Timeout is 100–120000 ms, default
30000. Commands are `--help`, `--version`, `list [--available]`,
`install RELEASE [--activate]`, `default RELEASE`, `rollback`,
`which voidc|voidls`, `verify RELEASE`, `doctor`, and
`run RELEASE|default voidc|voidls -- literal-args...`. No `-h`, `update`,
`self-update`, dependency-add command or implicit latest-release selection
exists in this parser. “Update” means explicitly installing and selecting a
release using those commands. Published versions are retained; no pruning,
PATH or shell changes are part of its interface.

`manager.up_execute` obtains the private root/writer lock and validates retained
state. `up_install` authenticates catalog/inventory, downloads with exact
bounds, extracts/verifies members and CRCs, publishes, saves control state,
then announces installation. Optional activation verifies and commits its
selection separately. `transport.up_download` is synchronous: it returns
download counts after the operation; there is no live byte callback here.

Reuse the status vocabulary and layout specification in a small VOID app
module, not through a new C++ FFI bridge. Use console.stderr capabilities and
time.monotonic_now/elapsed with the existing checked result handling. The
trust clock (`time.now_utc`, expiry/rollback checks) remains untouched.
Possible actual status boundaries: Fetching metadata; Verifying metadata;
Downloading artifact; Verifying/extracting; Publishing; Selecting default.
Never claim “installed” before content publication **and** state registration,
or “activated” before up_activate's successful state commit. Downloaded bytes
are not verified installed bytes. No live percentage/ETA without an actual
existing observable count; do not add a transport callback in this stage.

Preserve all stdout data (especially `which`'s single path and forwarded
voidls framing) and JSON stderr errors. Default redirected execution adds no
status. Because current error consumers parse the entire stderr document,
plain status must be an explicit separate opt-in; tooling should not combine
that opt-in with its old stderr-JSON parser. Do not launch a renderer around
forwarded tools. No global flag may be consumed after `run ... --`.

`up_environment` intentionally allowlists child environment names and sets
PATH itself. Do not broaden it to forward arbitrary presentation/SDK overrides;
in particular NO_COLOR is not currently forwarded. The recommended first
version is entirely plain, so no environment change is required. Future color
propagation would need a narrow review rather than bypass this policy.

Native/provider authorization stays in existing compiler/native policy.
Signing threshold, keys, origin policy, TLS, no redirects, size/hash/CRC,
revocation/freshness, lock ownership, interruption/recovery and activation
contracts stay unchanged. See the existing
[VOIDUP integration report](../voidup/INTEGRATION_REPORT.md) and
[G65 approval-reuse contract](G65_EDITOR_NATIVE_APPROVAL_REUSE.md).

### VS Code F5

Current F5 is already build-before-launch on supported Linux/Windows x64:
the generated preLaunchTask resolves `debugBuild`; projectInvocation('build',
true) forces project profile debug and passes --reuse-native-approval.
The child uses pipes, so automatic TTY status will not appear. The extension
can explicitly request `--status plain` after compiler support is verified;
do not inject new flags into older installed compilers based only on the
unchanged 0.1.0 version string. Introduce a scoped capability handshake or
require a known supporting compiler identity in that implementation stage.

Keep compile diagnostics unprefixed and contiguous. The matcher needs
`PATH:LINE:COLUMN: severity: message` immediately followed by
`    code: VOID...`; do not insert status between those records. Stream
decoding and CRLF handling in projectTask.ts must remain lossless across
chunk boundaries. It currently merges independently piped stdout/stderr in
arrival order; exact global write ordering cannot be reconstructed from two
pipes. Preserve per-stream order and existing diagnostic multiplicity.

Compiler's build wall time is the only compilation duration. An optional
extension `performance.now()` measurement from spawn to close can be labelled
“compiler process wall time”; it includes startup/shutdown and is not a phase
measurement. Task setup (project selection/dirty checks), debugger adapter
startup and target runtime are separate and should be omitted unless directly
observed. A stopped debug session includes user pause time and is not a
benchmark of program runtime. Do not report the F5 interval as compilation.

Continue using real nonzero task exits, never text such as Finished, to decide
build success. Existing generated launch uses preLaunchTask; the extension
checks workspace trust, dirty state, selected root/entry, program existence,
unsafe debug.onTaskErrors policies and post-substitution path agreement.
Preserve the [existing stale-image failure tests](../../editors/vscode/src/test/starterLaunchHost.ts#L60)
and [G64 behavior](G64_VSCODE_LAUNCH_WORKFLOW.md), with G65's newer reuse-only
approval path. No settings, launch files or recovery copies are changed here.

Upstream evidence narrows the remaining launch guarantee issue. VS Code
1.144.0's task runner rejects a returned nonzero/undefined exit under both
`showErrors` and `abort`; showErrors itself is not a known bypass. However,
its separate slow-task notification after ten seconds offers **Debug Anyway**
and can resolve a synthetic exit 0 while the task is still running. Its task
exception path also has an independent Debug Anyway choice. These mechanisms
are already present upstream, not a speculative new bug investigation.
[VS Code 1.144.0 task runner](https://github.com/microsoft/vscode/blob/1.144.0/src/vs/workbench/contrib/debug/browser/debugTaskRunner.ts#L61).

VOID's current extension checks the setting and program path/existence but
does not require a receipt from this exact compiler child. An old executable
can satisfy existence checks. Therefore those checks alone cannot establish
that every host override waits for a successful required build. This is a
source-backed risk, **not a reproduced G75 editor failure**. Preserve the
ordinary nonzero-task protections and qualify the slow-task/exception override
with an old executable in the bounded F5 stage. Require a successful actual
compiler-close receipt tied to invocation/root/entry/artifact before returning
the final debug configuration, fail closed if pending/failed/cancelled, and
invalidate stale receipts when a new build/selection starts. The upstream
debug service calls the substituted-configuration provider after the task
runner and before session creation, making the existing provider a defensible
place for the final check. Concurrent/repeated launches need distinct receipt
identity and tests.
[VS Code debug service](https://github.com/microsoft/vscode/blob/1.144.0/src/vs/workbench/contrib/debug/browser/debugService.ts#L505).

Do not change user settings or add a confirmation prompt to solve this. Boss
must approve the stronger launch guard as a bounded behavioral correction.
Windows adapter displayed exit statuses have known limitations in G64; they
do not replace compiler task exits. Upstream documents
[preLaunchTask](https://code.visualstudio.com/docs/debugtest/debugging-configuration)
and [multiline problem matchers](https://code.visualstudio.com/docs/debugtest/tasks#_defining-a-multiline-problem-matcher);
the latter require consecutive matching lines.

Share a **format/schema specification and fixtures**, not one mandatory binary
dependency across C++, VOID and TypeScript. C++ timing/format code belongs in
compiler tooling support with no Runtime ABI exposure; LLVM/semantic code
only emits optional observations. VOIDUP owns its small VOID formatter.
VS Code displays compiler records and preserves diagnostics; it should not
implement compiler phase inference or another spinner.

## 10. Performance, correctness and security risks

| Risk | Required control / decision |
| --- | --- |
| Existing scripts consume exact output | Default non-terminal bytes unchanged, all existing stdout schemas preserved. Top-level help content is the intentional exception requested here. Explicit opt-ins have documented output changes. |
| Instrumentation overhead | Two monotonic samples per measured scope, plus counter/sink work; N scopes imply at least 2N clock reads, not zero cost. Automatic total needs only start/end samples; disabled detail must avoid string building, per-node storage and callbacks in hot loops. No measured ns/percentage overhead is claimed by this review. |
| Short builds slow due to presentation | No polling thread, per-file updates, sleeps, terminal input or full renderer. Write a bounded number of status lines. Measure whole-process cost as well as recorded build time, since rendering/report writes are outside the latter. |
| Misleading benchmark precision | Record host/toolchain/build identity, application profile and effective optimization, actual cache status, nominal/effective clock resolution, warm/cold definitions and sample count. No phase subtraction pretending to isolate O2 or codegen. |
| Warm/cold ambiguity | Fresh CLI process is not cold OS cache. Distinguish first project cache use, repeated project cache, repeated in-process LLVM initialization and OS page-cache state. Do not drop system caches or mutate installed cache state during this assignment. |
| Timing changes compilation | No observer state in semantic/native identities, compatibility serialization, output IR or cache keys. Observer cannot authorize, retry, skip checks, lower twice or change optimizer/codegen. Compare meaningful output/IR/artifact oracles at implementation time. |
| Duplicate/lost diagnostics | One producer for each diagnostic; preserve current exception/warning order and child inheritance. Status sink never takes ownership of errors or captures all child output to prettify it. |
| Terminal injection and unavailable capabilities | Escape new untrusted labels, plain fallback, independent stderr eligibility. A denied/closed endpoint never makes compilation fail merely because new cosmetic status could not be written. Existing output failures keep existing semantics. |
| New timing file can overwrite trusted input | Exclusive, checked publication; protected stores/collisions rejected, no default files or raw application argv. A failed report is visibly failed telemetry without changing application status. |
| Security authority migrates to UI | Observation-only API, unchanged approval callback/store/digests, native ABI and signing/integrity policy. No new interactive approvals. |
| Shared helper grows into framework | Small command registry/formatter and optional bounded timing sink; independent app adapters. No new Runtime ABI, asynchronous terminal owner or build dependency. |
| Platform assumptions | LLVM/Clang/LLD 23.1.4 only. Local Linux source review is not Windows/macOS execution evidence; existing support limitations remain. |

Bounded performance qualification after implementation: reuse compiler_project
and compiler_phases with the same frozen bytes and oracles; compare baseline,
automatic status, detail off/on and JSON serialization. Use paired/interleaved
runs, at least the established 20 repetitions for the representative project,
plus a tiny source and failure cases. Report median, p95, worst, absolute ms
and paired percent change, cache state and raw sample count. Benchmark
redirected and real terminal lanes separately. Microbench clock/sink cost in
the same optimized toolchain, but do not substitute it for whole-process cost.
Do not publish thresholds as measured results. Any repeatable substantial
regression returns to Boss with the original unfavorable samples retained.
This is a concrete implementation check, not a new optimization investigation;
[G08](../compiler-health/G08_COMPILER_PERFORMANCE.md) and existing holds remain intact.

## 11. Required regression coverage and platform matrix

Existing executable contracts are mostly Python/C++ assertions and inline
expected strings, not a single golden-output directory. Relevant fixtures
include tests/fixtures/diagnostics and tests/frontend/diagnostics.hpp.
CTest registrations are in CMakeLists.txt (CLI around 575, project suites
around 402 onward); no registration changes are part of this review.

| Coverage | Reuse / required additions during implementation |
| --- | --- |
| Help and parser | [cli/contract.py](../../tests/cli/contract.py): all top aliases, first identity lines, resources-independent metadata, status/stream/error contracts. Add all fourteen command help pairs and help COMMAND, every accepted nested form, all observed flags/aliases, dynamic width after a synthetic longer command, spaces/no tabs, narrow/plain output. Preserve currently rejected help forms unless explicitly approved. |
| Invocation and forwarding | Existing CLI tests plus [os tests](../../tests/os): duplicate/missing/conflicting flags, negative legacy bounds, -- placement, literal --timings/--help app args, empty/space/Unicode args, Windows wide argv. Assert old exit classifications. |
| Machine output | [dependency_explain.py](../../tests/project/dependency_explain.py), [templates.py](../../tests/project/templates.py), editor creation/library fixtures: existing JSON schemas, error placement/bounds, edit tokens, list/suggest/root results; no status/ANSI added. voidls framing remains byte-clean. |
| Profiles/cache | [profiles.py](../../tests/project/profiles.py), [automatic.py](../../tests/cache/automatic.py), compiler_project oracles: default release, explicit debug, --no-opt without relabeling, custom debug/O2, hits/misses/unavailable/disabled, Windows no-persistence behavior. Existing exact --cache stderr survives default pipes. |
| Diagnostics | [diagnostics.cmake](../../tests/integration/diagnostics.cmake), [utf8_diagnostics.py](../../tests/integration/utf8_diagnostics.py), diagnostic fixtures: success warnings, multiple errors, invalid UTF-8, Unicode/space paths, linker failure; identical codes/content/order/count and matcher continuity. |
| Timings | Meaningful boundary/order assertions with a controlled clock/sink; no exact real-duration goldens. Check nested aggregates, no double count, no phases on hit/O0/check, partial failure state, null/unavailable semantics, bounded reports, elapsed units and invariant nonnegative durations. Validate both JSON examples and actual version-1 reports. |
| Timing transport | Concurrent source/app stdout and stderr, inherited linker diagnostics, JSON parsed only from separate file, report collisions/reparse/symlink/protected-store rejection, report I/O failure after successful build, interrupt/crash with missing report. Existing build/app exit remains unchanged. |
| Native/integrity | [approval_console.py](../../tests/project/approval_console.py), [managed_native.py](../../tests/project/managed_native.py), [maintenance.py](../../tests/project/maintenance.py), package/import/native debug suites. Approval IDs/records unchanged, reuse-only refusal unchanged, no prompting from task, integrity checks and failed-publication preservation intact. |
| VOIDUP | [tests/voidup](../../tests/voidup): smoke/error JSON, acceptance literal forwarding, signatures/revocation/timeouts, interruption/recovery, failed activation, stdout which, voidls protocol, exact child statuses. Status cosmetic failures cannot alter trust state. |
| VS Code | config/projectTask hosts, [starterLaunchHost.ts](../../editors/vscode/src/test/starterLaunchHost.ts), [nativeApprovalHost.ts](../../editors/vscode/src/test/nativeApprovalHost.ts): clean F5, compiler/spawn/late-publication failure with old binary retained, cancellation, changed selection, pipes, split UTF-8/CRLF chunks, problem matcher, old compiler feature negotiation, showErrors failure behavior, slow-task and exception Debug Anyway overrides with pending/failed builds, concurrent launch receipt isolation. Observe no adapter start without successful required task. |

| Native qualification lane | Required evidence | G75 review result |
| --- | --- | --- |
| Linux x64 GNU | Release compiler and Debug compiler; Debug/Release application modes; fatal ASan/UBSan/leak controls where supported; terminal, pipes, files, CI, TERM=dumb/unset, absent terminfo, closed stderr, Unicode paths; actual LLDB DAP F5 | Source reviewed; metadata-only probes of an existing build. New behavior **UNQUALIFIED**, unimplemented. |
| Windows x64 MSVC | Pinned complete SDK, /MT; Debug/Release, native console and Windows Terminal/ConPTY, redirected handles, long/Unicode paths, interruption, same output protections; actual cppdbg/GDB F5 | **UNQUALIFIED** for G75; cross compilation is insufficient. Preserve known adapter-status limitations. |
| macOS native supported targets | Native compile/check/JIT/AOT as already supported, pipes/TTY and clock behavior; unsupported source-debug/package operations retain explicit failures; no generated F5 promise | **UNQUALIFIED** for G75. Linux does not qualify macOS. |
| Other existing supported host architectures | Same plain-output/no-capability dependency and timing-unit checks; no new native FFI, debug, package or F5 support implied | **UNQUALIFIED** until native execution evidence exists. |

No production or native regression suite was run for a documentation-only
change. Existing historical passes remain evidence for their original changes,
not proof of this unimplemented proposal.

## 12. Bounded implementation stages after approval

1. **Help metadata and pages.** Extract presentation strings/command records,
   add dynamic padding, all current command pages and accepted nested pages.
   Preserve dispatch/parser behavior. Pass metadata-without-resources, help,
   aliases, all existing CLI usage and editor JSON tests. Deliver an isolated
   reviewable help change; no timing, library or output-status change.
2. **Outer wall timing and static status.** Add invocation-local clock and
   outcome observation around existing check/build/emit/package and JIT compile
   boundaries, separate invocation timing, proposed status policy. No detailed
   frontend/backend instrumentation yet. Pass stream/exit/failure/cache and
   native-approval contracts plus baseline/off/auto/plain cost comparison.
3. **Opt-in bounded phase observations and report file.** Add only section 6's
   real scopes, versioned schema and exclusive report publication. Keep ORC
   codegen/link combined and lex/parse combined. Pass JSON/failure/collision,
   cache-hit/unsupported-phase and output-equivalence controls; measure detail
   and serialization overhead. Stop rather than expand into pass profiling.
4. **VOIDUP formatting adapter.** Reuse the specification through existing
   console/time facilities; add only explicit applicable status policy around
   current operations. Pass trust, stdout/stderr JSON, interruption, forwarding
   and activation suites. No self-update, dependency manager or transport UI.
5. **VS Code pipe status integration.** Verify compiler capability, request
   plain status, preserve matcher/stream forwarding; distinguish compiler
   interval from debugger/app. Qualify successful-task gating with real F5
   including slow-task/exception overrides and stale-artifact failure. The
   proposed actual-child receipt guard is a separately reviewed correction
   within this stage, not implied implemented
   behavior. Native Windows and macOS limits remain explicit.

Each stage can be tested and reviewed independently. No spinner or new TUI
dependency is a hidden sixth stage. No optimization, language, native ABI,
package-security or G69 work is included. Boss/Wrench review follows the
existing workshop process; this document does not add an approval ritual to
ordinary already-authorized implementation choices.

## 13. Decisions requiring Boss approval

| Decision | Recommended approval |
| --- | --- |
| Terminal approach | Dependency-free newline status; no FTXUI/Indicators or animation in G75. |
| Help content | Compact top Options shows only help/version; command flags move to full command pages, all accepted forms/aliases/internal contracts retained. Explicitly update tests for intentional help text changes. |
| Default timing visibility | Automatic interactive totals, unchanged default redirected stdout/stderr. CI gets totals by explicit --status plain or --timings. If automatic CI totals are mandatory instead, approve a documented stderr compatibility change first. |
| New syntax and report transport | Command-scoped --status and --timings/--timings-format/--timings-output as specified; JSON requires a separate fresh file, no stdout/stderr JSON mode. |
| Telemetry failure contract | Invalid report path fails before build via usage handling; late report I/O failure is visible on stderr but does not replace build/app exit or destroy a successfully published artifact. |
| Timing precision and attribution | Inclusive wall scopes with null unavailable data; lex/parse and ORC materialization remain combined; no subtraction-based invented phases or global LLVM tracing. |
| Nested help extensions | Implement focused text for accepted forms now. Any newly accepted nested -h/help route is a deliberate grammar/exit change, requiring an explicit exception to the no-default-exit-change constraint. |
| F5 success guarantee | Add invocation-bound actual-compiler success gating against the upstream slow-task/exception override paths, then qualify it in the real editor; never change user settings automatically. Keep unsupported hosts and adapter-status limits explicit. |

The unresolved risks are bounded: platform behavior, measured observer cost,
safe report destination admission, older-compiler capability negotiation and
the host's slow-task/exception debug-launch behavior. None requires an open-ended new
compiler investigation before deciding this architecture. No semantic,
optimization, native ABI, authorization or package-integrity change is
recommended for approval.

## 14. Review evidence and exact changed-file inventory

Read-only checks performed:

- `git status --short` was clean initially; `git rev-parse HEAD origin/main`
  returned the supplied baseline twice. `git show --stat HEAD` identifies G74's
  public-API/completion work. No fetch, staging, commit, push or branch change.
- Repository file inventory, source reads and targeted symbol searches covered
  the paths cited above. No AGENTS.md was present in the ancestor paths or
  relevant source/docs/app/editor/tool/test trees. ripgrep was unavailable;
  tracked-file inventories and grep were used instead.
- Twenty-four metadata/help-only subprocess probes used the existing
  `build/llvm23-release/voidc` with captured streams. Its version reported LLVM
  23.1.4, x86_64-unknown-linux-gnu, build identity
  `f839256d25b5f989f5732bd643dfa440dfba76927a568b187f56c53f913e53bf`.
  The four top help aliases matched, 3636 stdout bytes, no stderr/ESC bytes.
  Other probes covered all fourteen command --help forms and five accepted
  nested forms. All had no ESC bytes, but remove --help, package install --help
  and new install --help returned 2 in this binary despite help branches in
  current source. This binary is **not a verified G74 build**. Its mismatches
  are preserved as a stale-artifact observation, not current-source regressions
  or passing G74 CLI qualification. No compiler was rebuilt for this review.
  Reproduction: invoke that exact path with [], -h, --help, help, --version;
  each of the fourteen known commands followed by --help; then
  `deps explain --help`, `deps fetch -h`, `native import --help`,
  `package install --help`, and `new install --help`.
- Upstream documentation and source linked in section 8 were read online.
  The live LLVM doxygen page identified itself as 24.0.0git; applicability was
  checked against the pinned 23.1.4 source and local SDK header instead.
  An Open Group isatty page was inaccessible (HTTP 403); no claim depends on
  that unavailable page. No upstream patch/dependency was integrated.
- Final document validation checks local link targets, JSON examples, command
  inventory coverage, spaces in the proposed command table, whitespace and
  the changed-file inventory. No build, install, real project run, terminal
  mutation, VS Code settings edit or launch/recovery modification was needed.
  Results: 55 local link targets exist; both complete JSON examples parse and
  contain the required fields; fourteen parser commands are covered; all
  fifteen displayed command descriptions begin at zero-based column 15 with
  no tabs. Markdown fences and trailing whitespace checks pass.
  `git diff --check --no-index /dev/null docs/projects/G75_CLI_EXPERIENCE_ARCHITECTURE.md`
  reports no whitespace errors. `git diff --name-only` and
  `git diff --cached --name-only` are empty; `git ls-files --others --exclude-standard`
  identifies only this new review document.

Exact intended changed-file inventory:

| File | Change |
| --- | --- |
| `docs/projects/G75_CLI_EXPERIENCE_ARCHITECTURE.md` | New architecture decision, proposal and review evidence. Unstaged and uncommitted. |

No disposable build outputs or downloaded dependency trees were created.
Existing build trees, installed tools, user data, settings and recovery
artifacts were left in place. This review is complete at the architecture
decision boundary and ready for independent Golden Wrench/Boss review.
