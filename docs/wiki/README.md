# Wiki maintenance and publication preparation

Repository-only instructions. **Do not copy this file into the GitHub Wiki.**
The public pages teach developers; engineering status and review evidence belong
in [the G89 report](../projects/G89_GITHUB_WIKI_OVERHAUL.md).

## Source and publication set

The accepted source baseline is G88 `553fe3f028ca3ce55d950c0bfe61b8d4503f13b3`.
The canonical language contract is [the Language Bible](../LANGUAGE_BIBLE.md);
these pages explain it through small examples rather than repeat the specification.
The previous 16-file draft inventory is preserved in the engineering evidence and
Git history. Existing page names are retained; new topics extend that structure.

[publication.json](publication.json) lists exactly **26 publication files**:
24 topic pages plus `_Sidebar.md` and `_Footer.md`, with exact SHA-256 values.
`Home.md` is the landing page. `README.md`, `examples.json`, `publication.json`,
reports, source fixtures and verification tools are **not** Wiki pages.

Every publication link is either an absolute `/wiki/PAGE` URL or a complete
repository `blob/main/PATH` URL. No `.md` topic routes or repository-relative
paths need translation at publication. In-text concept links and next steps
supplement the complete grouped sidebar. No images or assets are needed.

All public files must remain educational. Do not put Clanker names, milestone
labels, Boss/approval language, review history or unpublished-draft banners in
any publication page or shared sidebar/footer. Feature names may be described as
planned without engineering project numbers. Keep evidence here in the repository.

## Check and export locally

No network writes, Git mutations or publication occur in these tools:

```sh
python3 -B tools/wiki/check.py
python3 -B tools/wiki/prepare.py --output /absolute/fresh/private/wiki-export
```

The output directory must not exist. It contains `pages/` with only the 26
allowlisted Markdown files, plus a separate `publication.json` receipt.
After intentional page edits, run `prepare.py --refresh-inventory`, then check
again. Refreshing hashes does not itself establish content or example acceptance.

To reproduce the complete example checks, supply a matching existing build,
admitted native catalog and prepared editable raylib consumer. Do not prepare or
install an unrelated provider just to run this gate. Replace these illustrative
paths with the actual retained paths for the reviewed build:

```sh
python3 -B tools/wiki/check.py \
  --compiler build/llvm23-release/voidc --build build/llvm23-release \
  --catalog build/g24-physical/catalog-release \
  --raylib-project /absolute/existing/editable-raylib-consumer \
  --output /absolute/fresh/private/wiki-check
```

The checker extracts all 34 displayed VOID programs into private fixtures,
checks each, executes synchronous programs at O0/O2, and emits/links/runs both
async examples through the matching existing adapter. It also tests imports,
feature on/off selection, representative native AOT output and project commands.
Compiler/emitter/linker/native children retain 1 MiB soft/hard stacks and fatal
ASan/UBSan/leak settings. The output retains source, commands, binary/catalog
hashes and results, including failures. Native tests reuse prepared artifacts;
installation, device and cross-platform qualification are separate.

`tests/cli/learning.py <compiler> <matching-build>` retains the original canonical
introductory example executions and now invokes the Wiki structural checker.
The expanded Wiki's full execution gate is the explicit command above; it needs
the retained provider fixture rather than pretending one is universally available.

## Live state observed during preparation

Read-only checks on 20 November 2026 found `has_wiki: true`, but the public Wiki
URL redirected to the repository and `git ls-remote` of the Wiki URL returned
“Repository not found.” No published page or Wiki Git history was retrievable.
This is compatible with an uninitialized Wiki; it is not proof that no private or
historical content exists. The evidence contains the responses and original drafts.
No page was created and no Wiki setting was changed.

## Publication procedure — after separate authorization only

G89 authorizes preparation, not publication. Independent review must complete,
and Boss must separately authorize any repository integration and Wiki
publication. The following describes future work; none of its remote writes has
been performed by preparation.

1. Identify the approved repository revision and page inventory. Verify every
   page hash against `publication.json`, run the structural checker, and prepare
   a fresh export. Canonical `blob/main` targets must exist in the approved public
   repository before Wiki publication. Preserve the example receipts and limits.
2. Recheck the live Wiki URL and its Git refs read-only. If no repository is
   available and publication is authorized, the owner may need to create the
   initial **Home** page in GitHub's UI using the prepared `Home.md` bytes. Saving
   that page is publication and must be covered by the explicit approval.
3. Clone the available Wiki to a fresh owner-private directory. Discover its
   actual default branch with `git ls-remote --symref WIKI_URL HEAD`; do not assume
   `main` or `master`. Record its HEAD, full file inventory and content hashes.
   Preserve a verified backup before replacements. Do not create an unrelated
   orphan history to bypass an unavailable Wiki.
4. Compare every proposed filename with the current published inventory. Preserve
   unknown pages and assets. Review existing same-name content, including Home,
   sidebar and footer; replace it only if the publication approval explicitly
   covers that replacement. If remote contents differ from the reviewed state,
   review the new differences before proceeding. Never use a mirror or deletion
   sweep to make the live Wiki match the local directory.
5. Copy **only** the 26 files listed in the approved publication inventory from
   the export's `pages/`. Verify destination hashes, inspect the complete diff,
   and run `git diff --check`. Do not copy the repository-only README, JSON
   manifests, receipts or engineering report. Stage the exact allowlisted paths
   and make the authorized Wiki commit on its discovered default branch.
6. Fetch again, confirm that the observed remote branch can fast-forward to the
   prepared commit, then push normally to that branch. No force push, history
   rewrite or unreviewed merge. If concurrent edits prevent the push, preserve
   them and review the conflict rather than overwrite them.
7. Verify the remote SHA and clean local checkout. Visit the rendered Home page,
   sidebar, footer, code fences, tables and representative in-text/anchor links.
   Record actual hosted rendering separately from prepublication local checks.

GitHub's documented conventions:
[local Wiki Git workflow and filenames](https://docs.github.com/en/communities/documenting-your-project-with-wikis/adding-or-editing-wiki-pages),
[custom sidebar and footer](https://docs.github.com/en/communities/documenting-your-project-with-wikis/creating-a-footer-or-sidebar-for-your-wiki).
