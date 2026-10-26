# Compiler documentation contract

## OFFICIALLY ACCEPTED AND LOCKED: 26 October 2026

Boss confirms Golden Wrench source review and final hosted acceptance at tested
commit `94b00c682314fb239ba2dfdd99c02df87ba0a8af`.
[GitHub Actions 40271408951](https://github.com/Shmellyorc/Void.Language/actions/runs/40271408951),
`ci-link-experiment.yml`, mode `thinlto-cache`: **7/7 jobs passed, attempt 1;
1,421/1,421 CTests**. Ubuntu Debug/Release/Sanitizers passed **205/205 each**,
Windows Debug/Release **199/199 each**, and macOS ARM64 Debug/Release **204/204 each**.
Documentation-specific LSP and execution tests passed in all seven configurations;
hosted Debug editor integration passed on Linux, Windows and macOS. Local focused
acceptance remains **57/57**, with additional editor and stack evidence.

[The acceptance record and client/version limits](DOCUMENTATION_HOVER_IMPLEMENTATION.md)
retain the exact scope and historical receipts. This is a separately accepted
tooling/documentation repair. Stage 11's lock and historical LLVM DSE observation
are preserved. The complete contract below remains in force; no next-stage
engineering is authorized. This documentation lock awaits Golden Wrench
**documentation review**, not implementation acceptance.

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
