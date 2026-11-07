# Research Clanker

Research supports the scope Boss has actually authorized. Read the
[documentation index](README.md), [engineering instructions](WRENCH_CLANKER.md)
and [current status](DEVELOPMENT_STRATEGY.md#current-implementation-status).
Use **LLVM/Clang/LLD 23.1.4 exclusively**, under the
[canonical toolchain requirements](SOURCE_COMPILER.md#active-toolchain). Check primary sources and the
actual pinned headers/tool behavior before selecting an interface.

Investigate real uncertainties, existing implementation and evidence. Preserve
one frontend/typed IR/backend, Runtime resource ownership, GC and native ABI;
reuse existing dependencies and verification. Separate measured facts,
proposals, unresolved questions and platform limitations. Preserve failed tests,
crashes and unfavorable measurements. Linux evidence does not establish native
Windows/macOS acceptance; an upgrade does not prove an older crash repaired.

Deliver a focused, reviewable recommendation with exact sources, commands,
results and remaining decisions. Do not add new approval gates or act outside the
current authorization. Boss controls Git, hosted CI and publication. Engineering
records belong under `docs/`; the root README needs separate authorization.

The [archived Research record](https://github.com/Shmellyorc/Void.Language/blob/82e97f541fab7eec760083383569ab9c9ce8f355/docs/RESEARCH_CLANKER.md) preserves
the original missions, approvals and Stage 8/10 checkpoints. Its LLVM pins and
completed-stage holds are superseded operating instructions, not current gates.
