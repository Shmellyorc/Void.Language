# Compiler documentation contract

Boss approved full implementation on 25 October 2026 after Golden Wrench review.
This bounded repair follows the nine dependencies in
[the roadmap](DOCUMENTATION_HOVER_ROADMAP.md). The research and roadmap retain their
original proposal status as history. Stage 11 and its evidence remain locked.

Only contiguous declaration `##` lines are collected. Ordinary text describes the
symbol. A line beginning with `@param NAME DESCRIPTION` describes an explicit
formal; `@return DESCRIPTION` describes a value result. Tags inside sentences are
ordinary text. No other documentation syntax or rendering language is introduced.
Existing attachment to fields, types and other symbols is preserved.

Callable hover/completion contains the compiler signature, description and
`Returns: DESCRIPTION` when the callable returns a value. Parameter descriptions
and raw recognized tags are excluded. Signature help includes all existing labels
and binding metadata, but documentation only on the active parameter. An empty or
undocumented active parameter returns null for **automatic and manual** invocation,
including requests without trigger context. There is no manual exception.

No parameter tags, empty-only tags and unknown-only tags produce no missing-tag
cascade. At least one known nonempty parameter description enables one warning
per missing explicit formal. Unknown names and duplicate names warn at their tags;
duplicates relate to the first tag. The first nonempty description wins, including
for repeated return tags. Missing documentation alone never warns. Validate original
headers once, including unused generics and methods, without analyzing unused
bodies. Warnings are nonfatal through check, JIT, emit and linked native builds.

The compiler owns extraction, source spans, validation and result/enum metadata.
Clients consume plaintext and existing LSP responses; they never parse source tags.
Documentation stays outside compatibility contracts and does not require reload.
Actual popup lifecycle verification is separate from a protocol null assertion.
Results and platform/client limits belong in the implementation handoff.
