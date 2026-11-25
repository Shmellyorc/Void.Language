# G89 — GitHub Wiki overhaul

20 November 2026. **Public-content correction completed locally; focused independent
Golden Wrench re-review pending.**
Accepted baseline: `553fe3f028ca3ce55d950c0bfe61b8d4503f13b3` (G88).
No staging, commit, push, installation, Wiki setting change or publication is
performed. Boss controls repository integration and later Wiki publication.
This implementation chat does not claim its own independent acceptance.

## Existing Wiki state and draft inventory

The original `docs/wiki/` had 16 files: 13 topic pages, `_Sidebar.md`, `_Footer.md`
and a repository-only README. All original files are preserved in Git at the
accepted baseline and copied byte-for-byte into the private evidence directory.
All 13 existing topic filenames remain in the proposal; 11 topics are added.

Read-only live inspection found the repository public, default branch `main`,
and `has_wiki: true`. The Wiki URL redirected to the repository. A read-only
`git ls-remote https://github.com/Shmellyorc/Void.Language.wiki.git` returned
128, “Repository not found.” The GitHub connector does not expose Wiki endpoints;
ordinary read-only HTTP/Git checks supplied this evidence. No published page or
Wiki history was retrievable. This is compatible with an uninitialized Wiki,
not proof that there was never any published/private content. Publication must
reinspect and preserve whatever exists then.

The existing drafts had draft banners, repository-relative `.md` Wiki links,
legacy numeric-entry tutorials and stale descriptions of specification/editor
status. They were improved in place rather than discarded wholesale. The
canonical source specification remains authoritative and unchanged.

## Proposed structure and exact publication inventory

The publication set is **24 topic pages and two shared files**, with names,
absolute Wiki routes and SHA-256 values in
[`tools/wiki/publication.json`](../../tools/wiki/publication.json). The repository-only
README, example manifest, inventory JSON, engineering report and tools are not
published. There are no image or asset dependencies.

| Learning area | Publication pages |
| --- | --- |
| Start | `Home.md`, `Installation.md`, `First-program.md`, `Editor-setup.md` |
| Foundations | `Language-basics.md`, `Types.md`, `Functions.md` |
| Data and authority | `Structs-and-initialization.md`, `Managed-memory.md`, `Enums-and-flags.md`, `Errors-and-nullable-values.md` |
| Reuse and traversal | `Generics-and-interfaces.md`, `Static-members.md`, `Collections.md`, `Iteration.md` |
| Concurrency and selection | `Async.md`, `Conditional-compilation.md` |
| Application workflow | `Projects-and-modules.md`, `CLI-commands.md`, `Native-compilation.md` |
| Libraries | `Standard-library.md`, `Packages-and-raylib.md` |
| Help | `Troubleshooting.md`, `Resources-and-status.md` |
| Shared navigation | `_Sidebar.md`, `_Footer.md` |

Home provides task-based entry points. The real `_Sidebar.md` groups every topic;
`_Footer.md` supplies Home/help/specification/resource routes. Each topic also has
at least two distinct contextual Wiki destinations before its next-step footer.
Readers can follow a concept directly where it appears instead of searching the
sidebar. Cross-page anchors and next-step links are verified against prepared
page headings. Links use full `/wiki/PAGE` routes and full repository `blob/main`
URLs, so no publication-time `.md` link rewriting is needed.

Boss's later readability instructions govern the public set: direct explanations,
small practical programs, expected outputs and the next useful action. The
standard-library page supplies three examples and a task map to all 29 modules;
it does not copy the API inventory. Public pages contain no Clanker chat,
milestone labels, Boss/approval language, review evidence or draft banners.
The structural checker rejects named internal markers, including sidebar/footer
content; a full prose scan also checks for internal testing/acceptance narrative.
Automated keyword checks alone do not establish that content boundary.

## Documentation and example improvements

There are **34 complete displayed VOID examples**, identified in the
repository-only [`examples.json`](../../tools/wiki/examples.json). The checker extracts
actual page bytes; there is no separate handwritten copy of each program that
could silently diverge. The original hello/learning source examples and root
README remain unchanged.

The guides cover main-based applications, functions and receiver methods,
managed/value sharing, fallible initialization, numeric/tagged/flags enums,
results/nullable values, generic interfaces, fixed arrays/lists/dictionaries,
static members, custom/async iteration, conditional features, imports, parsing,
file/JSON use and native image ownership. Setup/workflow pages use practical shell
or settings examples rather than large API dumps.

The async examples explicitly require the existing root adapter. Ordinary CLI
run/build is not presented as an async host. Conditional selection is distinct
from runtime branching; feature maps remain package-owned. G93 `compile func`
remains planned, G92 completion improvements remain planned, and compact inline
`if` remains deferred. The public pages describe these boundaries by feature
name, without milestone numbers.

Raylib guidance uses the modern types, all 305 preserved values and 21 enum/flags
types. It explains `.value` at primitive boundaries, unknown flag bits, closed
numeric membership, native ownership, target-specific locks and consent. All
seven raw keyboard companions are listed, with F13/F25 codes and queue-to-query
use, invalid-query behavior, zero exit-key disabling and nullable name ownership.
The headless query example does not claim actual keyboard delivery. API source,
resource descriptors, native providers and vendor inputs are unchanged.

The Bible's signed MIN/-1 wrap and zero remainder are correctly taught; no compiler
semantics change is introduced. The historical Bible, all 50 frozen stdlib/package
inputs and G88 evidence remain unchanged. Automation-event APIs are not renamed
or given an invented enum.

Installation guidance uses the maintained source installer and exact SDK pin.
Editor guidance separates `voidc`, `voidls` and the local VSIX, uses current
manifest workflows, literal executable settings and actual configuration limits.
It does not claim universal recovery, automatic server download or Marketplace
availability. No installer, npm installation, extension install or fresh editor
launch was performed in this task.

## Verification and retained evidence

Evidence root: `/home/shmellyorc/Documents/Void.Language-archives/relocated-documents-2026-11-25/void-g89-waxdoi8k`.
The existing Release build and native catalog were reused without relinking or
preparing providers. LLVM/Clang/LLD 23.1.4 remains pinned. Compiler/emitter/linker/
executed native children use 1 MiB soft/hard stacks and zero core limits.

| Check | Result |
| --- | --- |
| Publication structure | 24 topic pages, two shared files, exact hashes, sidebar completeness, contextual navigation, all 34 code-block registrations and 115 unique destinations passed. Repository paths/fragments resolve; future Wiki routes resolve to the prepared inventory. |
| Displayed examples | 138 process commands passed in the final complete run (14.53 s launcher wall). All 34 source blocks checked; synchronous examples ran at O0/O2. Both async programs emitted, linked and executed through the existing adapter in normal/optimized modes. |
| Real workflows | The same gate exercised project creation, check/run/Release build, an independent executable, library creation/check, two-file imports, feature true/false selection, reused native raylib calls and representative AOT output. Version plus 11 command help routes passed. |
| Original introductory coverage | Updated `tests/cli/learning.py` passed the original 12 execution cases and two canonical README/first-program block identities, plus the Wiki structural checker. |
| GitHub Markdown | The documented stateless Markdown rendering API returned HTTP 200; it did not write a Wiki page. All 26 publication files rendered with 24 topic titles, eight tables, 51 fences and exact preservation of all 34 VOID program blocks. This is GFM rendering evidence, not a published Wiki UI observation. |
| Export | Exact 26-file allowlisted export and hashes verified. A repeat export to the same destination was refused; original bytes remained intact. The final export excludes all repository-only records. |
| Protected inputs | Root README, canonical/historical Bible, Wrench instructions and all 50 audited stdlib/package hashes equal G88. Final displayed program bytes equal the successful execution receipts. |
| Git state | Baseline HEAD retained; index empty; `git diff --check` passed. Exact changed inventory and source hashes retained in final-state evidence and the candidate archive. |

The execution harness retained the original fatal environment:

```text
ASAN_OPTIONS=detect_leaks=1:halt_on_error=1:abort_on_error=1:exitcode=87
UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1:exitcode=86
```

These commands used the Release compiler, not a newly qualified sanitizer build.
No new full sanitizer or compiler regression qualification is claimed. The logs
contain no ASan/UBSan/leak diagnostic; setting the fatal environment does not
substitute for instrumentation. No unrelated qualification suite was rerun.

Reproduction:

```sh
python3 -B tools/wiki/check.py
python3 -B tools/wiki/check.py \
  --compiler build/llvm23-release/voidc --build build/llvm23-release \
  --catalog build/g24-physical/catalog-release \
  --raylib-project /home/shmellyorc/Documents/Void.Language-archives/relocated-documents-2026-11-25/Void.Language-work/g88-review-corrections/vscode-workspace \
  --output /absolute/fresh/private/wiki-check
python3 -B tests/cli/learning.py build/llvm23-release/voidc build/llvm23-release
python3 -B tools/wiki/prepare.py --output /absolute/fresh/private/wiki-export
```

The full gate requires an existing matching test build, admitted catalog and
editable raylib consumer; the consumer is copied only into task-private storage.
`artifacts.json` identifies compiler/server/emitter, Clang/LLD and executed native
outputs. `examples-third/results.json` preserves actual source, commands, output,
stack/environment and catalog identity. `learning-final.log`, `remote-state.json`,
`github-markdown-render.json`, `markdown-inspection.json` and export receipts
separate execution, remote observation, rendering and export evidence.

### Original failures preserved

The first example run retained 66 commands and refused `pass` as a struct field
body (VOID1102). Its new async source was corrected to carry an explicit count.
The second retained 82 commands and refused a newly written deferred receiver
close without mutable receiver storage (VOID2018). The tutorial now uses the
established `with io.open(...)?` scoped-resource idiom, exercised with an actual
successful file fixture. Neither required a compiler or API change.

Early structural/HTML inspection mistakes are recorded in `early-findings.txt`:
a missing contextual destination, a checker filename comparison and an invented
minimum table count. These were corrected rather than weakening code validation.
Original failed results/logs and source programs remain separate from the final
successful run.

## Publication readiness and safe procedure

### Focused public-content correction

The original independent review is preserved at
`/home/shmellyorc/Documents/Void.Language-archives/relocated-documents-2026-11-25/void-g89-independent-review-w38kqp5t/review.json`.
It returned **CORRECTION REQUIRED** for PUBLIC-1: local testing/qualification
narrative in Installation, Editor setup and Resources and status. Its independent
138-command example check, original 12 learning cases, archive replay and other
successful checks remain evidence for that original candidate, not acceptance of
the corrected candidate.

The Boss authorized the focused correction within G89. Installation now directs
readers to their platform's prerequisites and commands. Editor setup describes
matching native executables and editor configuration. Resources and status
describes capabilities, planned work and actual platform/backend/portability
limits. The complete publication-set scan also found qualification wording in
Packages and raylib and Troubleshooting; these passages now explain device/backend
availability and platform-specific tools. Resources' heading is now **Platform
limits**; its two incoming links in Installation and Native compilation were
updated. No new API or tutorial scope was added. Native-provider consent remains
public workflow guidance.

The removed qualification history remains repository-only: the original
Installation text disclaimed fresh Windows/macOS installation testing; Editor
setup limited recorded local Linux VS Code/VSCodium observations; Resources
distinguished local source/editor and documentation/example checks from native,
keyboard, graphics/audio, hosted CI and performance acceptance. The package and
troubleshooting wording made similar qualification distinctions. The original
archive, original evidence root and independent review preserve those exact
texts and their supporting records unchanged. The limits in **Remaining limits**
still apply; no new native or hosted acceptance is claimed.

Correction evidence root:
`/home/shmellyorc/Documents/Void.Language-archives/relocated-documents-2026-11-25/void-g89-public-correction-3uf2d469`.
Focused structure/navigation checks retain 24 tutorials, 34 registered programs
and 115 unique destinations. All code fences, including all 34 VOID programs,
remain byte-identical to the prior candidate; program bytes also match both the
original successful execution and independent execution receipts. The 138 example
commands, original 12 cases and unrelated compiler tests were **not rerun**.
The full 26-file public set was scanned and the corrected prose reviewed for
developer guidance, with native-provider consent and runnable contributor adapter
instructions retained. Fresh stateless GitHub Markdown rendering, export hashes,
archive CRC/member/source identities and real G88 patch replay are recorded in
the correction receipts. This implementation does not claim independent re-review.

Relative to the reviewed candidate, exactly eight repository files changed:
the six public files Installation, Editor setup, Resources and status, Native
compilation, Packages and raylib, and Troubleshooting, plus `publication.json`
and this report. The exact overall G89 inventory below remains **34 files**.

Corrected local export:
`/home/shmellyorc/Documents/Void.Language-archives/relocated-documents-2026-11-25/void-g89-public-correction-3uf2d469/publication-final/pages`.
Its separate `publication.json` identifies exactly the 26 corrected Markdown files.
The original export at
`/home/shmellyorc/Documents/Void.Language-archives/relocated-documents-2026-11-25/void-g89-waxdoi8k/publication-final/pages` remains intact.

The complete repeatable procedure is in
[`docs/wiki/README.md`](../wiki/README.md#publication-procedure--after-separate-authorization-only).
It requires independent review and separate Boss publication authorization,
reinspection of live pages/refs, initialization of Home only if actually needed,
backup and explicit replacement approval, preservation of unknown pages/assets,
copying/staging only allowlisted page bytes, a normal fast-forward Wiki push and
remote/rendered verification. It discovers the actual Wiki default branch and
refuses force pushes, deletion sweeps or an invented orphan history. This task
executes none of those publication steps.

Official references used for bounded publication decisions:
[Wiki Git workflow and filenames](https://docs.github.com/en/communities/documenting-your-project-with-wikis/adding-or-editing-wiki-pages),
[sidebar/footer conventions](https://docs.github.com/en/communities/documenting-your-project-with-wikis/creating-a-footer-or-sidebar-for-your-wiki),
[stateless GFM rendering](https://docs.github.com/en/rest/markdown/markdown).

Original reviewed candidate archive, preserved unchanged:
`/home/shmellyorc/Documents/Void.Language-archives/g89-github-wiki-candidate-2026-11-20.zip`.
Its SHA-256 remains
`4d094a262f8d895a948848d2f3b44591dcb3180d1b040093c37711351016bcaa`.
Corrected candidate archive, with embedded source/member hashes, retained original
evidence and focused correction receipts:
`/home/shmellyorc/Documents/Void.Language-archives/g89-github-wiki-candidate-public-correction-2026-11-20.zip`.
The new SHA-256 sidecar and correction-root `archive-verified.json` identify its
final bytes. Actual accepted-baseline patch replay and manifest/CRC verification
are recorded there. This report is part of the corrected archive and does not
include its enclosing archive's self-referential hash.

## Remaining limits

The PUBLIC-1 correction awaits focused independent re-review. Independent
acceptance and Boss integration/publication authorization remain pending.
No live Wiki was created or overwritten.
The hosted sidebar/footer UI and live page routes must be observed after eventual
authorized publication; stateless rendering does not prove that UI integration.
Live content may change before then and must be rechecked.

Windows/macOS source installation, editor and native execution were not freshly
tested. Existing qualification limits remain. Physical keyboard, strict
graphics/audio, hosted CI and performance remain unqualified where previously
reported. No binary release, portable toolchain, provider rebuild or installation
acceptance is implied. Existing G88 source, inventory, original failures and
acceptance records remain preserved.

## Exact changed-file inventory

<!-- G89_CHANGED_FILES -->

Exactly **34 files**.

```text
docs/README.md
docs/projects/G89_GITHUB_WIKI_OVERHAUL.md
docs/wiki/Async.md
docs/wiki/CLI-commands.md
docs/wiki/Collections.md
docs/wiki/Conditional-compilation.md
docs/wiki/Editor-setup.md
docs/wiki/Enums-and-flags.md
docs/wiki/Errors-and-nullable-values.md
docs/wiki/First-program.md
docs/wiki/Functions.md
docs/wiki/Generics-and-interfaces.md
docs/wiki/Home.md
docs/wiki/Installation.md
docs/wiki/Iteration.md
docs/wiki/Language-basics.md
docs/wiki/Managed-memory.md
docs/wiki/Native-compilation.md
docs/wiki/Packages-and-raylib.md
docs/wiki/Projects-and-modules.md
docs/wiki/README.md
docs/wiki/Resources-and-status.md
docs/wiki/Standard-library.md
docs/wiki/Static-members.md
docs/wiki/Structs-and-initialization.md
docs/wiki/Troubleshooting.md
docs/wiki/Types.md
docs/wiki/_Footer.md
docs/wiki/_Sidebar.md
tools/wiki/examples.json
tools/wiki/publication.json
tests/cli/learning.py
tools/wiki/check.py
tools/wiki/prepare.py
```
