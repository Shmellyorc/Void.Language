# G62 custom VOID project templates

Baseline `7a18607ae68fd2cb5b1fc54f61265c0aaa6aa237` (G61).

A template is a normal source project with metadata at `.void/template.toml`.
Install it from a directory or an HTTPS Git repository, then select it by a
short name. This uses the existing user-directory policy, directory
publication, verified Git snapshots, manifest parser and project creation
identities. The .NET template workflow provided the model for listing,
selection and name replacement. [Microsoft project template tutorial](https://learn.microsoft.com/en-us/dotnet/core/tutorials/cli-templates-create-project-template)

## Authors

From the VOID repository root, copy the maintained example to start your own
template. The source directory can be a Git repository root or a GitHub
subdirectory.

```sh
cp -R tests/project/template_fixture ./VoidGameTemplate
cat ./VoidGameTemplate/.void/template.toml
voidc check --project ./VoidGameTemplate
```

```toml
format = 1
id = "org.example.voidgame"
name = "VOID 2D Game"
short-names = ["voidgame", "game-starter"]
description = "Starter project for a 2D VOID game"
tags = ["Game", "2D"]
source-name = "TemplateGame"
version = "1.0.0"
```

Use the schema 1 project manifest for an application or library. Declare
source roots and, when supplied, entries and application resources that exist
in the source tree. Add project dependencies and native requirements after
generation. The template cannot supply an existing `void.lock`, nested
project manifest, or files under project administration directories. The
template metadata is the one permitted `.void/` input. A local checkout's
top-level `.git` is omitted. Shell scripts and other ordinary files can be
included and are copied as data.

The format is UTF-8 TOML up to 16 KiB and accepts the seven required keys shown
above plus optional semver `version`. Extra keys, duplicate keys, mismatched
types and unknown formats are rejected. The portable ID is limited to 96 bytes.
Names and tags are single-line text. Friendly names are limited to 128 bytes,
descriptions to 256 bytes and 128 Unicode scalars, and tags to 32 bytes each.
Supply one to eight short names and one to twelve distinct tags. Short names
start with a lowercase letter and use at most 32 ASCII letters, digits and
hyphens. They may not end in a hyphen or reuse built-in starters or CLI command
words. The built-in names `app`, `classlib` and `lib` are reserved.
An ID and every short name must be unique in the store.

The `source-name` and generated `--name` are portable VOID identifiers of at
most 64 ASCII letters, digits and underscores, starting with a letter. Make
the placeholder distinctive. The compiler substitutes exact, case-sensitive
occurrences in path components and UTF-8 text files with extensions `.void`,
`.toml`, `.md`, `.txt` and `.json`. Those files reject NUL and invalid UTF-8.
Other file contents are copied byte for byte. For example, the maintained
binary asset contains the literal placeholder bytes, which survive even
though its filename changes. Each generated project has a fresh project ID
and application or package ID. The project display name follows `--name`.
Generated library package names use the lowercase form. Authored manifest
settings such as version, resources and optimization profiles carry through.

## Consumers

```sh
voidc new install --dir ./VoidGameTemplate
voidc new list
voidc new voidgame --name MyAwesomeGame
voidc new game-starter --name SecondGame
voidc run --project MyAwesomeGame
voidc new uninstall voidgame
voidc run --project SecondGame
```

For a GitHub-hosted template, substitute your real repository in either
example. Both commands select a commit internally.

```sh
voidc new install --git https://github.com/OWNER/REPOSITORY
voidc new install --git https://github.com/OWNER/REPOSITORY/tree/main/templates/game
```

The store is `%LOCALAPPDATA%\VOID\templates` on Windows,
`$XDG_DATA_HOME/void/templates` on Linux with the default
`~/.local/share/void/templates`, and
`~/Library/Application Support/VOID/templates` on macOS. Short names match
exactly. Uninstalling through any short name removes the whole registration.
Existing generated projects can still run. A verified identical registration
can be installed again. Uninstall before changing a template that has the
same ID.

Use `voidc new -- voidgame` for a literal directory named `voidgame`, or
`voidc new ./voidgame --name "My Project"` for the built-in application with
an explicit display name. Bare `voidc new` and `voidc new list` list
templates, including the permanent `app` and `classlib` starters.
`lib` is an alias for `classlib`. Bare `voidc new install` and
`voidc new uninstall` retain
literal-path creation. The template subcommands take a source selector or a
short name, respectively.

## Verification and limits

Local template source admits only regular files and physical directories,
up to 8,192 entries, 64 MiB of bytes and the existing directory-depth limit.
Git input uses D3's existing transport, commit and tree verification,
storage limits and GitHub slash-ref selection. The registration receipt
binds source bytes and the canonical local source path or immutable Git
origin, commit and subtree. Git-backed registrations retain the verified
snapshot. List, generation and uninstall verify stored inputs before using
them. The registry admits up to 64 templates.

Generation checks the resulting portable paths, rejects case-insensitive
collisions, revalidates the generated manifest and verifies the bytes in a
private stage. The existing publisher uses an exclusive directory rename
into a fresh destination. A failure before publication leaves the
developer's files intact. No template code runs during install or
generation. Executing the generated source continues to require the
normal project and native authorization checks.

## Qualification

- Linux x86-64 Release and fatal ASan/UBSan/LeakSanitizer each passed six
  selected CTest tests. The focused template lifecycle also passed with a
  1 MiB Linux stack limit. Both compiler builds passed the separate
  bundled raylib workflow.
- On both build profiles, the maintained fixture generated two applications
  with distinct project and application IDs. Each printed the expected
  name and score in normal and O2 JIT and in separately linked normal
  and optimized AOT. Both generated binary assets matched the authored
  bytes. Two simultaneous local install processes left a single verified
  entry. Duplicate names, invalid metadata, reserved paths, a physical
  symlink, substituted filename collisions and tampered registrations
  were rejected. The fixture verified that uninstall leaves an existing
  project runnable. A separate Linux check instantiated a library with
  its own project and package identities.
- The existing TLS fixture exercised root registration on the production
  compiler. A separate test compiler substituted only the Git transport
  endpoint to exercise the GitHub slash-ref subtree URL over the same TLS
  fixture. The receipt retained the exact canonical GitHub origin, commit
  and selected directory. No suitable public fixture repository was
  available for an independent live public install.
- VS Code 1.141.0 and the candidate Release language server passed four
  generated-project checks for import navigation, clean diagnostics,
  completion with an unsaved diagnostic, and refresh after a disk edit
  and restoration. The same project checks ran directly against
  `voidls` in the platform fixture.

- Windows 10 x64 build 19045 passed five selected CTest tests per
  compiler profile, Debug and Release, plus the direct package and
  raylib workflows. The maintained fixture ran generated projects with
  normal/O2 JIT and independent AOT on both compilers. The compiler,
  language server, project creation helper and generated executables
  kept the 1 MiB PE stack reserve. The final guest verification checked
  3,354 inputs and the unchanged pinned SDK and providers. The runner's
  original final checksum expected the old fixture import. The
  verification replay used the recorded fixture correction, which had
  been applied before both profiles ran the tests.
- The Linux and Windows tools used LLVM, Clang and LLD 23.1.4 at
  `7a941dfb2877a0b8cf30323fefa85ad019e0b0f5`. The macOS storage
  path is implemented. Native macOS qualification remains open.

## Warm startup

The accepted raylib project printed its exact pixel oracle for every
sample. Six balanced blocks per mode compared G60, G61 and G62 after
the qualification jobs completed. Values are median milliseconds to
first output, with the full sample range in parentheses.

| Mode | G60 | G61 | G62 | G62 paired change vs G61 |
| --- | ---: | ---: | ---: | ---: |
| Default | 674.19 (627.05–703.06) | 659.90 (636.57–680.97) | 650.34 (634.90–666.60) | −0.98% |
| O2 | 648.51 (635.84–719.10) | 634.43 (620.47–657.69) | 641.91 (628.02–681.57) | +0.83% |

Whole-process paired changes versus G61 were −1.06% and +0.94%,
respectively. The ranges overlap. The O2 shift is an adverse measurement
in this sample, with no material warm-startup regression established.
The raw samples and compiler identities are in the archive.

**Recommendation: INTEGRATE.** The source and test inventory is verified
against the G61 parent. Positive public GitHub acquisition and native
macOS qualification remain open.


The initial fixture used bare `use parts`, which resolves to the standard
library. The corrected fixture uses an explicit relative import. The
Linux build view also had to use physical license and notice paths for
the existing application packaging test. The initial and corrected
receipts are in the evidence archive.

## Built-in starter review

The permanent built-ins are `app` and `classlib`, with `lib` retained
as a library alias. The named commands create the directory, manifest
and starter source together. The library includes a public function
and its own package identity. Positional destinations and literal
`new PATH` remain available. A destination of `.` uses the existing
directory after checking that it is empty. Publication uses the
same pinned creation stage and exclusive filesystem operations as
fresh creation. It publishes the manifest last and refuses unrelated
files discovered before publication.

The Linux x64 `app` starter includes the existing LLDB DAP launch
recipe. Build the saved Debug project before pressing F5. The
generated configuration resolves the executable through the current
VOID project selection. Windows uses the existing Run Project command
or `voidc run .`. Library starters carry only library files.
[Editor workflow](../EDITOR_INTEGRATION.md#running-a-built-in-application-starter)

Linux Release and native Windows 10 x64 Release passed five selected
tests each. The selection covered creation, the process and editor
protocol, custom templates, package separation and HTTPS acquisition.
Three Linux creation and template tests passed with fatal ASan,
UBSan and LeakSanitizer. The Windows compiler and language server
kept the 1 MiB stack reserve. The pinned toolchain stayed at 23.1.4.
Real Linux VS Code 1.141.0 passed Run Project, Build Project and a
native launch by name from the generated configuration with the
official LLVM LLDB DAP extension. The Windows editor check exercised
`voidls` directly. The Linux test initially ran against an intermediate
listing label. A rebuild verified the final labels and command behavior.

**Recommendation: INTEGRATE.** The correction applies to the preserved
qualified G62 snapshot and is available as a separate review patch.
