# Wiki publication review

These files are proposed Wiki pages, prepared locally for Boss approval. No Wiki
repository has been cloned, written, committed or published. `Home.md`, the topic
pages, `_Sidebar.md` and `_Footer.md` are the publication set; this maintenance
file is repository-only.

Keep repository documentation canonical. Update examples/contracts there first,
then change only Wiki navigation/walkthrough summaries that need it. Links to
`blob/main` are intended for the eventual approved main-branch change, not a claim
that these uncommitted new documents already exist on GitHub.

The learning checker compares every displayed VOID block with its canonical
`examples/hello` or `examples/learn` file, executes synchronous cases in both JIT
modes, and freshly emits/links/runs async code with the existing test adapter.
`tests/cli/learning.py <compiler> <matching-test-build>` is also a CTest entry.
Do not maintain a second specification or copy large historical reports here.

Before any approved publication, review the final source revision and platform
evidence; remove the draft banner, translate local `.md` topic links to Wiki page
routes, check canonical links after the repository change is available, and
preserve existing Wiki material unless its replacement is specifically approved.
The current Wiki content has not been rewritten by this milestone. Boss selects
publication timing and any migration of existing pages.
