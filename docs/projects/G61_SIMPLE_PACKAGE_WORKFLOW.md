# G61 — Simple VOID packages and project commands

Accepted parent `dc93b675ed44f0fa37823a49eeecd0815b6ce940` (G60).
The candidate reuses the existing project writer, verified Git snapshots,
native adapter generator, native catalog and lock format.

| Command | Implemented behavior |
| --- | --- |
| `voidc new`, `voidc new list` | Show the built-in template names, short names and tags |
| `voidc new app [PATH]`, `voidc new lib [PATH]` | Instantiate the built-in console and library starters |
| `voidc new PATH`, `voidc init [PATH]` | Keep literal-path creation and initialization of an existing project |
| `voidc add ALIAS`, `voidc add ALIAS --dir PATH` | Copy a registered package, prepare bundled raylib, or add an explicit local library |
| `voidc add ALIAS --git URL` | Select repository HEAD or a GitHub tree URL, then record an immutable pin |
| `voidc tidy`, `voidc remove ALIAS` | Reconcile the project's existing dependencies or remove one direct edge |
| `voidc package install ALIAS --dir PATH`, `voidc package install ALIAS --git URL` | Install a verified library into platform user storage |
| `voidc package list`, `voidc package uninstall ALIAS` | List verified registrations and bundled packages, or remove a user registration |

`--path` remains an alias for `--dir`. User package registration and
uninstallation use `voidc package`. A literal path matching a starter
name can be passed to `voidc new` as `./lib` or `-- lib`.
`voidc run file.void` still works without a manifest. Package commands
retain the alias as the destination under `libs/` when they materialize
a copy.

## Automatic user package storage

Installation uses `%LOCALAPPDATA%\VOID\packages` on Windows,
`$XDG_DATA_HOME/void/packages` on Linux with the default
`~/.local/share/void/packages`, and
`~/Library/Application Support/VOID/packages` on macOS.
The compiler creates the store on demand. It serializes registration,
verification, copying and uninstall with the existing package writer
lock. It prepares each entry privately, verifies its lock and inventory,
then publishes it by directory rename. Uninstall verifies the entry
before removing it from the published store.

The registration receipt binds the requested origin, the store manifest
and lock, and the exact source and catalog bytes. Repeat installation
from the same source verifies the existing registration. A consumer
copies verified files and Git proofs, then publishes its own manifest
and lock through the existing journal. It can use its copy after user
uninstall. The store supplies neither a project path nor native execution
authority to the consumer. Ordinary compiler project and output commands
reject destinations inside the selected store. A user's operating system
permissions still govern direct access to their files.

`voidc remove` updates the project manifest and lock and removes a
verified registered or Git view. Installation from an explicit editable
local path retains that source on removal. Registered local views have
a receipt so removal can distinguish those copies from editable source.
[NuGet user package storage and concurrency](https://learn.microsoft.com/en-us/nuget/consume-packages/managing-the-global-packages-and-cache-folders)

## GitHub and native preparation

Repository URLs use the isolated Git transport. A tree URL checks exact
branch and tag ref names at each directory boundary with bounded
`git ls-remote` output. Annotated tags are peeled, slash-containing
refs are considered as a whole, and an ambiguous interpretation is
rejected. Acquisition then uses the exact object ID, verifies the commit
and tree objects and stores the revision and subtree in the existing
manifest and lock fields. The transport retains the existing TLS,
protocol, time and storage limits. [Git ls-remote reference](https://git-scm.com/docs/git-ls-remote.html)

A package with `native/void.build.toml` selects the declarative C11
recipe. The compiler stages the captured recipe inputs and builds one
provider source and its checked adapter with pinned Clang and LLD 23.1.4.
The physical declaration path, provider ID, ABI and target must agree.
It validates the object format and rejects uncontrolled linker
directives. A generated catalog records the descriptor, binary and
notice identities, and D4 verifies that catalog through the existing
lock and loading machinery. A valid repeat add reuses the catalog.
Other native build systems and additional native libraries remain
outside this recipe. [Package author guide](../NATIVE_LIBRARIES.md#declarative-c-packages) · [Clang command reference](https://clang.llvm.org/docs/ClangCommandLineReference.html)

## Approval and future templates

Execution still requires `--allow-native`. The Linux fixture demonstrated
that install, catalog reuse and build metadata alone leave both run and
build closed without that flag. A changed package source, changed
descriptor or changed provider binary must first pass the existing lock
verification.

A later durable-approval design can write an explicit user-owned
approval after a verified provider is displayed and the developer
consents. Its key should bind the application identity, verified
dependency lock, physical native declaration identity, native provider
ID/version/ABI and target. Put the record outside project-controlled
source, and admit it only after existing source and native verification.
An explicit CI approval input should bind the same identities and
fail closed when missing or different. `--allow-native` stays available
during migration. The G61 negative tests exercise the existing identity
checks that would precede approval lookup. The approval record and
interactive or CI interfaces are a design proposal.

### Proposed G62 template metadata

G62 proposes `.void/template.toml` at the template root. It records a
friendly display name, one or more author-defined short names and
descriptive tags. The list shows
all three columns, and instantiation selects by short name such as
`voidc new console` or `voidc new classlib`. Registration rejects
short-name collisions, including collisions with built-ins. Keep
literal `new PATH` available, and make GitHub and local template
registration and uninstall independent of application dependencies.
At instantiation, declarative project-name substitution replaces the
template's project-name placeholder with the chosen project name.
Template expansion runs without package scripts.
G61 ships only the built-in starter list and the existing explicit
library store commands. [Microsoft template metadata and CLI conventions](https://learn.microsoft.com/en-us/dotnet/core/tutorials/cli-templates-create-project-template)

## Original G61 qualification

Linux Release and fatal ASan/UBSan/LeakSanitizer each passed 15
selected CTest checks. The independent C fixture covered exact JIT and
AOT output in both profiles, missing consent, edited source and
binary/descriptor tampering, unchanged catalog identities and timestamps,
and removal of a Git-backed native package. The existing raylib workflow
passed for both Release and fatal sanitizers, including named install,
artifact reuse, native consent and independent AOT. Separate process
interruptions after displacing a view and after publishing the new
manifest and lock restored the exact original files. Real VS Code
1.141.0 passed 14 package events using the G61 compiler and language
server, including completion, hover, semantic tokens, package edits and
native declaration invalidation.

The Linux fixture used real Git commands and the exact resolver source
with a test-only origin substitution. Whole-repository and nested
package selection, slash refs, annotated tags and full commit URLs
produced immutable pins. Conflicting and absent refs failed and published
no lock. The unmodified production candidate's live GitHub checks
resolved the accepted VOID repo's branch or rejected an absent ref. Both
the live root acquisition and live nested acquisition on that repo
failed at D3's existing storage limit. The accepted commit's tree has
56,229 tracked entries and roughly 739 MB of tracked file bytes. The
small fixture is positive evidence for the URL implementation. A
successful end-to-end install from a small public GitHub repository
remains unqualified.

Native Windows 10 x64 build 19045 passed all nine selected tests
in each of Debug and Release with LLVM, Clang and LLD 23.1.4.
Both compiler profiles passed the separate bundled raylib workflow
and the independent C package workflow, including normal/O2 JIT,
independently linked AOT, native consent, tamper checks and
uninstall. PE headers confirmed the 1 MiB stack reserve for covered
compiler, language server, test and application executables. The guest
verified 3,344 exact inputs and preserved the pinned SDK and providers.

On the prepared G59 raylib project, eight interleaved pairs per mode
gave these median times to first output. Every run printed the same
pixel oracle.

| Mode | G60 median ms (range) | G61 median ms (range) | Median paired change |
| --- | ---: | ---: | ---: |
| Normal JIT | 636.44 (626.72–687.47) | 654.16 (632.37–689.52) | +0.58% |
| O2 JIT | 655.65 (640.29–726.14) | 666.06 (646.54–697.42) | +1.49% |

Whole-process median paired changes were +0.34% and +1.21%. The VM
qualification was active on the host, and the ranges overlap. The
replay retained G60's warm-launch gain against its measured G59
baseline. The original G60 visibility optimization and bundled raylib
sources remain byte-identical to the accepted parent.

## Final public GitHub check — 2026-11-17

The qualified Linux Release compiler ran
`voidc add mylib --git https://github.com/Shmellyorc/Coroutines.git`
against the actual public HTTPS endpoint. This 15 KiB repository was an
invalid-package control. GitHub reported HEAD
`b051a3bbdd031d0533a6fe18f3ca9a92c04b1915`, whose root lacks a VOID
manifest. The compiler reached package validation and rejected the
repository with exit 1. The consumer manifest stayed byte identical,
and the command published no lock, `libs/mylib` view or Git snapshot.
`voidc tidy --check` and `voidc check .` both passed afterward.
[Public control at the observed commit](https://github.com/Shmellyorc/Coroutines/tree/b051a3bbdd031d0533a6fe18f3ca9a92c04b1915)

**Live installation of a valid public GitHub package remains
unverified.** Boss confirmed that there is no known small public VOID
package for this check. The proposed standalone raylib repository
returned HTTP 404. The published VOID monorepo remains above the
existing D3 limits. Its live acquisition failures and the passing
HTTPS and subtree fixture evidence carry forward. Positive install,
repeat, import and execution on a suitable public package remain
unverified.
[Standalone package publication proposal](G24_PHYSICAL_SOURCE_IMPLEMENTATION.md)

Boss directed that the missing public fixture be documented without
holding G61. The D3 limits and native approval behavior remain in force.

## User store correction qualification

Linux x64 with LLVM 23.1.4 passed all ten selected project tests in
both Release and fatal ASan/UBSan/LeakSanitizer. The separate bundled
raylib workflow passed in both builds. These are focused reruns for the
user store correction; the earlier Linux, Windows and VS Code counts
above describe the original G61 candidate. Startup was not retimed.

The updated tests cover concurrent installation, source and receipt
tampering, failed install rollback, origin conflicts, and refusal to
redirect a project command into the store. The HTTPS fixture observed
zero new requests during repeat registration and project add. It
verified the original origin, exact commit and selected subtree in the
project lock, and executed the project after user-store uninstall. The
C fixture checked that neither registered native artifacts nor their
timestamps changed on reuse, and that native execution still requires
explicit consent. The project copy passed normal/O2 JIT and separately
linked normal/optimized AOT checks in the focused workflows.

Native Windows 10 x64 build 19045 passed all four selected tests in
both Release and Debug with LLVM, Clang and LLD 23.1.4. The direct
source and native package fixture and the separate raylib workflow also
passed in both builds. PE headers preserved the 1 MiB stack reserves
for the compiler, project creation helper and covered AOT applications.
The guest verified 3,344 build inputs and unchanged SDK and
provider hashes. The Release runner stopped after passing qualification
when it tried to hash an unbuilt `voidls`; the resumed runner kept the
Release receipts and completed Debug. At that qualification point,
the candidate matched the compiler and focused package fixture bytes.
Two other fixtures differ from the Windows build packet only in the
unexecuted macOS choice of which user data environment variable to set.

Golden Wrench approved G61 with a final CLI cleanup that confines
package registration and uninstallation to `voidc package`. Linux and
Windows qualification for the user store passed at the focused scope
above. The macOS storage path was implemented but not exercised on a
macOS host. Positive public GitHub installation remains unverified. The
successful HTTPS fixture and the live invalid-package control are
retained. G62 template metadata remains a proposal.

The final CLI cleanup passed five focused Linux Release tests covering
project creation, package registration and removal, project maintenance
and HTTPS Git acquisition. The tests check that the removed aliases
exit with a usage error without changing the store. The creation
fixture now checks that bare `voidc new` lists starters, matching G61's
existing behavior.

The user store correction's exact G60 diff, 19-file SHA-256 inventory
and qualification receipts are in `g61-user-package-store-2026-11-17.zip`.
The archive restores the postimages from before the final CLI cleanup
and verifies them by SHA-256.

The evidence archive `g61-simple-package-workflow-2026-11-17.zip`
preserves the original source patch, postimage hashes, replay
instructions, raw timings, CTest XML, editor evidence and native
platform receipts. The source patch restores from the G60 parent. The
supplemental live check and final source verification are in the
qualification evidence directory under `final-github/`.
