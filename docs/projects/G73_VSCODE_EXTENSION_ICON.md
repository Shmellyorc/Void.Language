# G73 VOID VS Code extension icon

Accepted parent `7ef5ba6d8354ba295ec0a3d63c320b259cfd326b` (G72).
GitHub main returned that exact revision during this review on 2026-11-18.
Work is on `codex/g73-vscode-extension-icon`.

## Artwork and packaging

The source is `website/dist/assets/mark.svg`. Its three paths retain their
original geometry and fills `#11172d`, `#6b779b` and `#303b64`.
The 256×256 RGB PNG uses the approved `#f5f8fd` background. Rasterization
expands the source viewBox uniformly from `0 0 44 44` to `-3 -3 50 50`.
This gives the visible mark approximately 25 pixels of horizontal margin and
40 pixels of vertical margin. The original proportions are retained.

The export used the existing FFmpeg librsvg decoder. A temporary SVG added
the background rectangle and expanded viewBox around the original paths.
The SVG source in the repository is unchanged. The export SVG and
`check-package.py` are in the evidence directory.

The top-level manifest `icon` is `icons/void-extension.png`.
The exact PNG is allowlisted in `.vscodeignore`. VS Code documents a PNG
of at least 128×128, with 256×256 for Retina screens.
[Extension manifest](https://code.visualstudio.com/api/references/extension-manifest),
[extension publishing](https://code.visualstudio.com/api/working-with-extensions/publishing-extension).

| Artifact | SHA-256 |
| --- | --- |
| Website SVG | `bd31769f264134d98c6ad2b60c679519b504f4a19cfc576214aa047e7b2736ab` |
| Extension PNG | `a52681a18b812b534ac2a88f8979ef95e628b7a3489eb2f732b792dccbcdd3a1` |
| Candidate VSIX | `f979c57f83590c9659788de57628f4eaab226f55eb8e4469e7cca5442b2590c0` |
| Saved G72 VSIX | `2d4dc83add18f3f49a814dce9e998cd96395fea3af0a62eee9248c710b150fda` |

The PNG is 8-bit RGB, with an opaque pale background and antialiased edges.
The source paths and original solid fill colors were checked independently.
Painted pixels fit within the exclusive bounds `(25, 40, 231, 215)`.
The VSIX contains ten files. ZIP integrity, packaged manifest equality,
the VSIX icon declaration and the exact packaged PNG bytes passed.
All existing extension payload files except the manifest match the saved
G72 VSIX byte for byte.

## Actual Linux VS Code checks

VS Code 1.144.0 was used with disposable profiles. A G72 baseline replay
showed the generic extension placeholder. The packaged G73 replay showed
the official mark in the Extensions list and extension details header.
Both Dark Modern and Light Modern passed. Screenshots were inspected.
The list displays the PNG at 36×36 and the header at 128×128. Both use
contain scaling and fit within their containers without clipping.

The candidate activated with the existing installed G72 server. In a copy
of the physical raylib project, typing `rl.` offered `set_target_fps`.
Semantic tokens stayed available, with 66 tokens before the edit and 64
during the incomplete selector. Problems showed `VOID1102` and cleared
after correction.

The extension build, typecheck and eight configuration tests passed.
The packaged-extension host suite passed trusted, Restricted Mode and
multi-root cases. Existing coverage exercised completion, semantic tokens,
navigation, trace controls, diagnostics, restart and failure handling.
Deliberate transport failure and SIGPIPE messages in that suite belong
to its failure-handling checks. Each editor invocation exited successfully.

This milestone's desktop results cover Linux VS Code. Windows and
VSCodium were outside this verification.

## Owner installation and recovery

The candidate was installed using VS Code's VSIX installer. Its version
remains 0.1.2 because this patch only changes extension artwork and metadata.
The candidate VSIX hash above identifies this build.

The installed directory is
`/home/shmellyorc/.vscode/extensions/void-language.void-language-0.1.2`.
Every installed payload file was compared with the candidate VSIX.
A fresh desktop test loaded that exact directory, displayed the icon in
both themes and repeated the raylib completion and diagnostic checks.
The actual server process resolved through `/proc` to
`/home/shmellyorc/.local/opt/void-g72-7ef5ba6d/bin/voidls`.

The installed compiler and server hashes match their pre-install values.
The owner's VS Code settings passed the before-and-after hash check.
Existing windows can use Developer: Reload Window to refresh cached
extension metadata.

`/home/shmellyorc/.local/opt/void-recovery-before-g73/restore` verifies
the saved G72 VSIX hash and reinstalls it. This was rehearsed in a disposable
profile, and every restored file matched the G72 VSIX. Reload the editor
after restoration. Earlier G72 tool recovery installations remain available.

Logs, screenshot pairs, package audits, export files and desktop receipts
are under `/home/shmellyorc/Documents/Void.Language-G73-evidence`.

## File icon review

G72 has no language-level icon on its `void` language contribution and
no file-icon theme contribution. G73 adds the extension's top-level icon.
The held G69 candidate adds separate language-level file icons.

G69's recorded failure is that its light language fallback overrides an
existing theme's explicit `*.void` mapping in VS Code and VSCodium.
G69 remains on HOLD. Revisit that work when both editors preserve explicit
theme overrides in the original replay.

### G69 status correction — 19 November 2026

Boss has superseded G69's blanket HOLD. The observations above remain historical;
normal theme fallback and the manually configured extension-mapping case are
distinguished in the [current G69 record](G69_VOID_FILE_ICONS.md#correction--19-november-2026).
The G73 top-level extension icon remains unchanged.

## Proposed commit inventory

- `editors/vscode/package.json`
- `editors/vscode/.vscodeignore`
- `editors/vscode/icons/void-extension.png`
- `docs/projects/G73_VSCODE_EXTENSION_ICON.md`

`git diff --check` passed. All four files remain unstaged.
The separate main checkout retains the owner's existing
`docs/WRENCH_CLANKER.md` edit.

Recommendation **INTEGRATE**.
