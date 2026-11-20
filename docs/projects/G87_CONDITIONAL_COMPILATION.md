# G87 — Conditional compilation

20 November 2026. **Corrected candidate; focused Golden Wrench re-review pending.**
Accepted baseline: `4f9b56ed4dc3be8e0fcc29d2b972b49b80446401` (G86).
Local Linux qualification is recorded below; no independent or hosted acceptance
is claimed. No staging, commits, branches, pushes, installation or publication.

## Source contract

```void
compile if target_os == "windows":
    func platform_name() -> string:
        return "Windows"
compile else if target_os == "macos":
    func platform_name() -> string:
        return "macOS"
compile else:
    func platform_name() -> string:
        return "Linux"

compile if feature.tracing:
    func trace():
        print("Tracing enabled")

func main():
    print(platform_name())
    compile if feature.tracing:
        trace()
```

The [complete example](../../examples/conditional/main.void) includes its manifest.
Run from the repository root:

```sh
build/llvm23-debug/voidc run --project examples/conditional --profile debug --no-cache
build/llvm23-release/voidc build --project examples/conditional --profile release -o /tmp/void-conditional
/tmp/void-conditional
```

Set `tracing = false` in the example manifest to exclude both its declaration and
its call. The same file remains the application entry.

Each directive occupies one physical line and ends in `:`. A trailing `#` comment
is allowed. Its nonempty source region is indented using existing spaces/tabs
rules. Alternatives align with their matching directive; dedenting ends a chain.
The selected region joins the enclosing scope, without an additional lexical,
visibility, module or initialization scope. Nested regions work at declaration,
struct/interface/enum-member and statement boundaries, including constructors,
static members, generic bodies and async functions. Directives cannot splice
individual tokens, expressions or ordinary argument continuations. An enclosing
language body that becomes empty still needs a selected ordinary statement such
as `pass`. `compile` is reserved; `#` remains a comment introducer.

Predicate grammar, highest precedence first:

```text
atom := true | false | feature.name
      | target_os (== | !=) "os"
      | target_arch (== | !=) "architecture"
      | (predicate)
unary := atom | not unary
conjunction := unary (and unary)*
predicate := conjunction (or conjunction)*
```

OS values are exactly `windows`, `linux`, `macos`; architectures are `x86_64`,
`aarch64`. Unknown predicate names, target values, feature names and malformed
conditions diagnose with source-owned `VOID5101`. Every operand and directive
is validated, including conditions under excluded parents and alternatives after
an earlier match. No short-circuiting can hide a typo. Limits are 64 nested
regions, 64 nested unary/group expressions and 512 predicate atoms. Existing
source, parser, import and diagnostic limits remain.

All source is UTF-8. Excluded ordinary physical lines are opaque: they need not
lex, parse, type-check or name available dependencies. Directive grammar, nesting,
body indentation and indentation style still have to be well formed. A first-token
`compile` on an excluded line is always structural, including between unmatched
quote characters in excluded text. In active source, existing comments and string
lexing apply; directive-looking lines inside active multiline strings remain text.
BOM, CRLF, tabs, Unicode strings and original byte/line/UTF-16 offsets are retained.
Ordinary `if`, constant `if` and `has` keep their existing two-branch checking rules.

## Configuration and ownership

```toml
[build.conditions]
tracing = true
experimental = false
```

A condition is a declared boolean, not a variable, macro, expression or build
script. Names contain 1–64 lowercase ASCII letters, digits or `_`, beginning with
a letter or `_`; the manifest permits at most 128. Duplicate keys, unknown build
fields, nested condition tables, wrong types and invalid names retain source-located
manifest diagnostics. Absence is an unknown-condition error, not implicit false.
The map is deterministic and independent of environment variables, profile names,
working directory and machine-local state. There are no CLI feature overrides.
Existing Debug/Release and check/run/build/emit conventions apply unchanged.

Project-owned VFS sources share the root manifest map. Locked packages retain
conditions from their own validated manifest and cannot acquire consumer features.
There is no feature unification or consumer override mechanism. Standard modules
have no consumer feature map. Standalone source has target predicates and boolean
literals, without project features. Declared manifest dependencies remain subject
to ordinary lock/admission verification even if their import is excluded; source
selection grants no package acquisition, filesystem or native authority.

The frontend derives OS/architecture from LLVM's default target triple, which is
also used by the existing native backend. Darwin/macOS and ARM64 spellings normalize
to `macos` and `aarch64`. This uses target identity rather than C++ host-platform
macros. Native initialization verifies that LLVM default and ORC process target OS/architecture agree, rejecting a mismatched SDK. Current JIT/AOT/CLI modes remain native-target-only; there is no frontend
flag that can choose a target independently of code generation. OS/architecture
predicates do not replace native ABI/descriptor checks. Metadata tests for Windows
and macOS triples are not native execution qualification.

## Implementation

`conditional.cpp` owns target normalization, canonical configuration keys, portable
feature names and the restricted bounded predicate parser. `conditional.inc` owns
an iterative per-lexer region stack. The normal lexer skips directive and inactive
lines before the one existing parser, import graph and semantic pipeline. Selected
lines subtract only directive-body indentation from logical indentation, without
rewriting source or moving token offsets. Active multiline strings and interpolation
continue through the original lexer. Generic syntax captures already-selected
tokens, so every specialization uses the same target/owner configuration.

Manifest D entry inventory, execution E, libraries, local imports, locked packages,
native declarations, editor document analysis and recovery use this same selection.
Inactive imports never perform VFS lookup or create module/dependency edges.
Selected declarations keep existing nominal identity, visibility, package boundaries,
initialization ordering, generics, ownership and native inventory validation.
Conflicting active declarations use the ordinary duplicate diagnostics. Excluded
globals and their initializer effects never reach typed IR or generated code.

Every analyzed module retains its normalized configuration and original-source
conditional regions. Configuration keys enter compatibility serialization; even a
feature change that preserves public signatures requires a Runtime restart. The
invalidator marks modules whose conditions changed. Project snapshot/cache keys
include target/condition identity and exact manifest bytes, and locked dependency
identity already includes package manifests. Existing cache/source/dependency and
snapshot freshness checks remain authoritative. Feature-only manifest edits refresh
analysis without requiring project reselection solely for the new feature map.

The language server indexes selected declarations/references and emits inactive
lines as semantic `comment` tokens with modifier `inactive`; directive lines use
`keyword`. Hover/navigation/completion expose selected symbols; inactive positions
have no definitions or completion candidates. Completion recovery uses the same
configuration. Unknown/incomplete conditions diagnose without crashing the server.
The VS Code grammar highlights `compile`, and themes may customize `comment.inactive`.

Formatting currently returns no edits for a source containing conditional regions.
This deliberately preserves authored indentation in every configuration; formatting
both selected and excluded branch syntax remains a tooling limitation. Condition
headers diagnose but do not yet offer condition-name completion.

## Design research

Research was completed before implementation, against primary upstream language
contracts, on 20 November 2026:

- [Rust conditional compilation](https://doc.rust-lang.org/reference/conditional-compilation.html):
  compiler-owned target predicates and externally configured conditions guided the
  closed, immutable configuration model. Rust attribute/macro syntax is not copied.
- [Swift compiler control statements](https://docs.swift.org/latest/documentation/the-swift-programming-language/statements/):
  target OS/architecture and boolean region selection informed the region model.
  VOID uses indentation and explicitly documents opaque excluded source instead of
  importing Swift's ordinary inactive-branch parsing rules.
- [C# preprocessor directives](https://learn.microsoft.com/dotnet/csharp/language-reference/preprocessor-directives):
  restricted conditional source selection supports the distinction from runtime
  branching. VOID retains `#` comments and introduces no textual macros or source
  symbol mutation.

- [LLVM ORC target detection](https://www.llvm.org/doxygen/JITTargetMachineBuilder_8cpp_source.html):
  `detectHost()` uses the process triple. The pinned SDK's `TargetParser/Host.h`
  separately documents the default compilation triple. VOID therefore rejects an
  OS/architecture mismatch between these two native targets before selection.

These are established upstream contracts, not proposed fixes or unmerged patches.
Their applicability is a frontend design reference; they do not prove VOID's target,
Runtime, native ABI or tooling behavior. LLVM/Clang/LLD 23.1.4 remains the exclusive
local toolchain. No competing frontend, Runtime, compiler execution facility or new
build system was introduced.

## Verification and evidence

Evidence root: `/home/shmellyorc/Documents/Void.Language-work/g87-conditional`.
The checked-in harnesses and task-local `qualify.py` / `run-editor.py` preserve exact
commands, environment, binary hashes, original failures and protocol/editor receipts.
Builds use the existing `build/llvm23-debug`, `build/llvm23-release` and
`build/llvm23-strict` configurations with a disk-backed task-local `TMPDIR`.
Compiler/server/emitter/native qualification children use a 1 MiB soft/hard stack
and disabled core dumps; builds and Electron use ordinary host limits.

Original pre-review candidate qualification on Linux x86-64 (preserved history):

| Profile | Focused CTest gates | Wall time |
| --- | --- | --- |
| Debug | 16/16 passed | 52.80 s |
| Release | 16/16 passed | 22.90 s |
| RelWithDebInfo, fatal ASan/UBSan/leak detection | 16/16 passed | 54.99 s |

**48/48 original unique gate outcomes.** These runs preceded the two confirmed
review findings and are not qualification of the corrected candidate. That original corrected strict run is
`strict-current-qualification.log/json`; Debug/Release final runs are
`debug-final-qualification.log/json` and `release-final-qualification.log/json`.
`final-state.json` verifies their recorded compiler/server/runner/test binary hashes
against the original candidate build outputs. Each profile's source harness passed 105 process
checks, including JIT/AOT O0/O2 and configuration-only cache changes. The runnable
example separately passed check, JIT O0, linked AOT O2 and native execution.

Original candidate editor receipts: real Linux **Visual Studio Code 1.144.0** and **VSCodium 1.143.1** both passed all
five editor observations: selected hover/definition/references, rendered inactive
semantic color, actual active-feature suggestion popup, unsaved manifest selection
changes, and incomplete-condition diagnostic/repair with a live server. The servers
ran through the 1 MiB wrapper. Their launch receipts match the original Release server
hash. The extension was loaded through `--extensionDevelopmentPath`, using isolated
profiles and an empty task-local extensions directory, without installing or
publishing an extension.

Original review archive (retained):
`/home/shmellyorc/Documents/Void.Language-archives/g87-conditional-2026-11-20.zip`,
with adjacent SHA-256 receipt. It contains the complete candidate, baseline patch,
restoration/member hash manifest, exact scripts, selected qualification/protocol/
editor receipts and original failed attempts. Patch replay is checked byte-for-byte
against all 34 original changed files using their accepted-baseline bytes; ZIP CRC and member
bytes are verified. `archive-verified.json` retains the archive digest and checks.
Rebuildable objects/binaries, caches, editor profiles and pinned SDKs are excluded.
No unrelated cleanup was performed.

The focused 16-gate selection is:

```text
^(conditional_(configuration|source|async|async_aot_normal|async_aot_optimized)|lsp_conditional|project_(manifest|selection|sessions|profiles|dependencies|protocol)|generics_source|generic_interfaces_source|custom_iteration_runtime|lsp_editor_recovery)$
```

Build targets:

```text
voidc voidls void_io_runner void_conditional_tests
void_conditional_async_normal void_conditional_async_optimized
void_project_manifest void_project_selection void_project_sessions
void_custom_iteration_tests
```

Fatal sanitizer environment:

```text
ASAN_OPTIONS=detect_leaks=1:halt_on_error=1:abort_on_error=1:exitcode=87
UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1:exitcode=86
LSAN_OPTIONS unset
```

Coverage includes active/inactive conflicting declarations, platform predicates,
boolean precedence and complete validation, nested regions and bounds, malformed
conditions/nesting/indentation, missing inactive local/package/standard imports,
selected globals and excluded initialization, generics/interfaces, managed/value
structs, enums/static members/constructors, native fixture admission and typed IR,
package-local conditions, configuration-only cache miss/warm hit, immutable editor
snapshots, stale-result rejection, reload restart, JIT/AOT O0/O2, async generic custom
iteration, actual LSP diagnostics/highlighting/navigation/completion/recovery and
real Linux editor clients.

Original unsuccessful attempts are retained: the initial build used an incorrect
internal package-accessor name; the first smoke command ran before the new compiler
had linked; test development corrected generic constraint spelling, qualified enum
presentation, expected diagnostic columns, duplicate-TOML wording and package
fixture version/enclosure/name rules. The initial editor TypeScript build required
async wrappers around VS Code Thenables. Initial Debug/Release integrated results
were 15/16 because of the incomplete package fixture; successful reruns are
recorded separately from those originals. The first strict integrated run passed 15/16 and failed the existing mixed-indentation
namespace completion assertion. Its build warning points to frontend line 8846,
while the corrected source places that statement at line 8851: the long compilation
had read the earlier source before an editing change and finished afterward. The
stale generated object's hash/timestamps and full failed log are preserved in
`strict-stale-object.json` and `strict-final-qualification.log`. Only that generated
object was marked out of date before rebuilding the unchanged corrected source.
No assertion was weakened or sanitizer setting changed. No compiler crash or sanitizer finding is
reclassified or discarded. No unrelated repository-wide suite or performance study
was run.

Native Windows/macOS execution, Windows/macOS editor behavior and hosted CI remain
**UNQUALIFIED**. Local Linux triple simulation and Linux editor runs cannot qualify
those platforms. Installation/publication and independent Golden Wrench acceptance
remain outside this candidate's authorization.

## Exact changed-file inventory

The original candidate changed 34 files; the corrected inventory is finalized below.

The original inventory was:

```text
CMakeLists.txt
docs/CLI.md
docs/DEVELOPMENT_STRATEGY.md
docs/GENERICS.md
docs/LANGUAGE_BIBLE.md
docs/MODULE_SYSTEM.md
docs/README.md
docs/SOURCE_COMPILER.md
docs/projects/G87_CONDITIONAL_COMPILATION.md
editors/vscode/README.md
editors/vscode/src/test/conditionalHost.ts
editors/vscode/syntaxes/void.tmLanguage.json
examples/conditional/main.void
examples/conditional/void.project.toml
include/void/conditional.hpp
include/void/diagnostic.hpp
include/void/frontend.hpp
include/void/project.hpp
include/void/project_resolution.hpp
include/void/tooling.hpp
src/frontend/compiler.cpp
src/frontend/conditional.cpp
src/frontend/conditional.inc
src/frontend/project.cpp
src/frontend/project_resolution.cpp
src/frontend/project_selection.inc
src/frontend/tooling.cpp
src/lsp/main.cpp
tests/conditional/api.void
tests/conditional/async.void
tests/conditional/editor.void
tests/conditional/lsp.py
tests/conditional/source.py
tests/conditional/tests.cpp
```

## Golden Wrench corrections — 20 November 2026

Golden Wrench returned **CORRECTION REQUIRED** for two P2 defects. Original review
and reproductions remain at
`/home/shmellyorc/Documents/Void.Language-work/g87-independent-review-g370ou41/`.
The original candidate archive and SHA-256
`815850434471a7f97555465258b8427471c563731b2efa60c4833435ca32456b` remain unchanged.
New correction evidence is under
`/home/shmellyorc/Documents/Void.Language-work/g87-corrections/`, including copied
original reproduction records and the complete review. No assertion or sanitizer
setting was weakened.

### Excluded multiline continuation

Previously, the normal lexer never saw parentheses in skipped lines, so the selector
incorrectly checked a continued argument as an ordinary indentation-sensitive line.
The selector now lazily records balanced lexical extents and preserves continuation
formatting across excluded calls and multiline literals. It stores source offsets,
without parsing expressions or introducing inactive declarations/imports/initializers.
Strings reuse the existing string lexer; conditional directive lines remain structural.
Framing uses a bounded delimiter stack and counts excess inactive nesting without
recursive parsing. Comments, literal delimiters and malformed opaque text retain
focused recovery coverage. The existing active expression parser is unchanged.

The exact reviewed `false` fixture now prints only `after`; its `true` counterpart
prints `value` then `after`. Tests also exercise closing `)` on the argument line
or its own line, nested calls, named arguments (`name`, `health`, `speed`), cosmetic
dedents, multiline literals containing source-looking globals, nested conditionals,
unknown conditions and 300 levels of inactive delimiter nesting. Parenthesized
arithmetic, boolean conditions and return expressions work through the existing
parser; unsupported declaration/type/generic headers are not broadened.

### Package inspection configuration

`inspect_native_provider` now requires explicit build conditions. Package maintenance
passes the selected library's validated map for editable local addition and verified
Git-view metadata inspection. A source comment or identifier containing `native`
therefore cannot lose declared features. Nested manifest boundaries are excluded
from the inspected owner's source inventory. This does not change native admission,
provider validation, descriptor inventory, linking or execution authority.

The new regressions cover comment/identifier controls, true/false library conditions
opposed to the consumer's values, conditional physical-provider advice, nested
project boundaries, and ordinary package execution. Fixtures use in-project editable
sources without package installation or native preparation.

### Boss suggestions, assessed separately

Both requested multiline call styles and multiline named arguments already work in
the active parser. They are retained and covered in ordinary, selected and excluded
forms. Existing parenthesized arithmetic, condition and return-expression support
is likewise retained; no parser extension was needed for these cases.

Optional `if ready: return` bodies are feasible as a small parser extension using
existing Statement and typed-IR control flow. They still need explicit rules/tests
for simple versus compound bodies, `else`/`else if` association, nested compact forms,
recovery, source extents and formatting. Statement and value-producing `if` have
separate parser paths with shared lexical layout/formatting. Recommendation: defer
compact statement bodies to future language-completeness work, preserving indented
bodies. No compact-if implementation or additional milestone is part of this repair.

### Focused correction qualification

Final corrected Linux x86-64 results, against the current binaries:

| Profile | Focused gates | Wall time |
| --- | --- | --- |
| Debug | 10/10 passed | 44.54 s |
| Release | 10/10 passed | 17.67 s |
| RelWithDebInfo, fatal ASan/UBSan/leak detection | 10/10 passed | 54.12 s |

**30/30 focused correction gate outcomes.** Each profile passed 59 new correction
process checks and the preserved 105-check source harness. LSP coverage now records
37 observations, including conditional multiline calls in both configurations.
The unchanged generic async source is qualified through JIT O0/O2 and both native
AOT modes. Existing editor-recovery, project-session and dependency checks pass.
`final-state.json` verifies each profile's recorded compiler/server/runner/unit-test
hashes against current binaries. No sanitizer memory finding or compiler crash
occurred in these correction runs.

Real Linux VS Code 1.144.0 and VSCodium 1.143.1 both passed six observations against
the corrected Release server, including unsaved feature switches around a multiline
call with cosmetic argument/closing-parenthesis dedents. The isolated development
extension setup and 1 MiB server wrapper are retained; neither client was installed
or published. The editor launch hashes match the corrected server.

The first new test attempt rejected a fixture that reused `player` as both a type
and a local variable, correctly preserving the existing no-shadowing rule. The
fixture type was renamed `player_state`. The original failed command, diagnostics
and input remain in `debug-corrections-1.json/log`; no compiler behavior or assertion
was weakened to make it pass. Intermediate successes and final runs remain separate.

The selected gates are:

```text
^(conditional_(configuration|source|corrections|async|async_aot_normal|async_aot_optimized)|lsp_(conditional|editor_recovery)|project_(dependencies|sessions))$
```

Compiler/server/emitter/native children retain the 1 MiB soft/hard stack and disabled
core dumps. Strict qualification retains the original fatal ASan/UBSan/leak settings.
LLVM/Clang/LLD 23.1.4 and the existing three build configurations are reused. This is
focused correction qualification, not a repository-wide rerun or comparative benchmark.
Windows/macOS execution and editor behavior and hosted CI remain UNQUALIFIED.
The conditional-file formatter still returns no edits. Everything remains unstaged;
no commit, push, installation or publication is authorized or performed.

### Correction reproduction and review bundle

Build targets:

```text
voidc voidls void_io_runner void_conditional_tests
void_conditional_async_normal void_conditional_async_optimized void_project_sessions
```

Run `qualify.py debug`, `qualify.py release` and `qualify.py strict` from the
correction evidence directory. It preserves exact CTest selection, fatal sanitizer
environment, 1 MiB soft/hard stack, disabled core dumps, task-local disk-backed
`TMPDIR`, binary hashes and complete output. The maintained focused harness can
also run directly:

```sh
python3 -B tests/conditional/corrections.py build/llvm23-debug/voidc /tmp/g87-corrections.json
```

The harness sets its compiler/native children's 1 MiB limit itself. For strict
runs, retain the fatal sanitizer environment listed above. `run-editor.py vscode`
and `run-editor.py vscodium` in the evidence directory retain actual native client
commands and the development-extension workflow. No installed extension is changed.

Corrected archive:
`/home/shmellyorc/Documents/Void.Language-archives/g87-conditional-corrections-2026-11-20.zip`,
with adjacent SHA-256 receipt. It contains the complete corrected candidate,
`candidate.patch` against accepted G86 and `corrections.patch` against the original
34-file G87 candidate, plus hash manifest, exact scripts, original reviewed
reproductions, new failure/success receipts and focused editor/qualification evidence.
Both patch paths are replayed byte-for-byte against all corrected files; ZIP CRC
and member bytes are verified. `archive-verified.json` records the result and digest.
The original candidate ZIP is retained with its original hash. Rebuildable binaries,
objects, caches, editor profiles and pinned SDKs remain outside the archive.

### Exact corrected file inventory

The corrected candidate changes 35 files against accepted G86. The focused delta
from the reviewed original candidate changes 11 of these files; the archive manifest
lists both sets explicitly.

```text
CMakeLists.txt
docs/CLI.md
docs/DEVELOPMENT_STRATEGY.md
docs/GENERICS.md
docs/LANGUAGE_BIBLE.md
docs/MODULE_SYSTEM.md
docs/README.md
docs/SOURCE_COMPILER.md
docs/projects/G87_CONDITIONAL_COMPILATION.md
editors/vscode/README.md
editors/vscode/src/test/conditionalHost.ts
editors/vscode/syntaxes/void.tmLanguage.json
examples/conditional/main.void
examples/conditional/void.project.toml
include/void/conditional.hpp
include/void/diagnostic.hpp
include/void/frontend.hpp
include/void/project.hpp
include/void/project_resolution.hpp
include/void/tooling.hpp
src/frontend/compiler.cpp
src/frontend/conditional.cpp
src/frontend/conditional.inc
src/frontend/project.cpp
src/frontend/project_resolution.cpp
src/frontend/project_selection.inc
src/frontend/tooling.cpp
src/lsp/main.cpp
tests/conditional/api.void
tests/conditional/async.void
tests/conditional/corrections.py
tests/conditional/editor.void
tests/conditional/lsp.py
tests/conditional/source.py
tests/conditional/tests.cpp
```
