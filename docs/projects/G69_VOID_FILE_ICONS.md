# G69 VOID file icons

> Storage correction, 2026-11-21: the historical `/tmp` evidence locations in this report have been cleaned. Retained Markdown and original failure records now use the [verified migration inventory](/home/shmellyorc/Documents/Void.Language-archives/storage-correction-2026-11-21/MIGRATION_INVENTORY.md). Redundant generated outputs and disposable fixtures were deleted under the Boss’s updated retention instruction. Historical commands and measurements below are unchanged.

**Current status — 19 November 2026: original blanket HOLD superseded by Boss.**
The standard language-icon contribution is being qualified on the accepted G76
baseline `da64cdbb99d535ab86da74521a6fc15bb72d5091`. The original G68
record and its custom-mapping observation remain below as historical evidence.

Original accepted baseline `92432510ebfe0844aae4e6285b8c4eff579ad137` (G68).

The existing `void` language contribution points to two 48×48 RGBA PNGs.
The dark appearance uses solid white and the light appearance uses solid
black. Both retain the exact three paths and 44×44 viewBox from
`website/dist/assets/mark.svg`. Only the fills change before rasterization.
The PNGs have matching alpha masks, including transparent backgrounds and
antialiased edges. Both silhouettes remain recognizable at 16×16 and 24×24.

| Asset | SHA-256 |
| --- | --- |
| Website SVG | `bd31769f264134d98c6ad2b60c679519b504f4a19cfc576214aa047e7b2736ab` |
| `void-dark.png` | `4cb06c9b39e0685b73408a73d08d0840a731ad6b36a6e3d5d14f0ad83854d2bd` |
| `void-light.png` | `d9f008214fadd23a0feee4e3cf3963b937f5daf89af1545a8c875b093bba9c64` |

## Original G68-based packaging and manifest (historical)

`npm ci`, `npm test` with the qualified G68 language server (8/8), and
`npm run package` passed on Linux. The VSIX contains both PNGs. Their
packaged bytes match the source files and their alpha matches a fresh
rasterization of the original SVG pixel for pixel. The language icon
object is the only change to the extension manifest. The existing manifest
and package were accepted by both desktop editors. The contribution follows
the [VS Code language default icon specification](https://code.visualstudio.com/api/language-extensions/overview#file-icon).

## Original desktop qualification (historical)

The same candidate VSIX was installed in disposable Linux profiles for
VS Code 1.144.0 and VSCodium 1.143.17232. Each desktop replay inspected the
rendered icons in Explorer and editor tabs for `main.void`, `player.void`,
`raylib.void`, and `app.void`. Settings changes were confined to the test
profiles. The test used the editors' built-in themes and Material Icon
Theme 5.40.0 as an existing override fixture.

| Desktop check | VS Code | VSCodium |
| --- | --- | --- |
| Seti with Dark Modern and Light Modern | White and black variants selected correctly | White and black variants selected correctly |
| Modern Icons with language icons disabled | Generic theme icon retained | Generic theme icon retained |
| Material Icon Theme `player.void` filename override | Retained in both appearances | Retained in both appearances |
| Custom Material Icon Theme `*.void → json` extension mapping (manually configured) | Retained in dark appearance. VOID fallback wins in light appearance | Same result |

The G68 VSIX displayed that exact extension override in both appearances
on both editors. The VS Code file icon stylesheet gives the light language
icon selector the same specificity as an unqualified extension selector
and places it later in the stylesheet. That explains the precedence change
when the language contribution is present. [VS Code 1.144.0 stylesheet generator](https://github.com/microsoft/vscode/blob/2c33b50b16c472efe58357d1d2a8a5414bc23fbf/src/vs/workbench/services/themes/browser/fileIconThemeData.ts#L347-L461)

## Original recommendation (superseded)

The following was the original recommendation; it is preserved, not the current status.

**HOLD.** The official language icon contribution works with Seti, but
the light appearance regression changes an existing `.void` theme override.
Release after both desktop editors give explicit extension mappings priority
over the light language fallback in this replay.


## Correction — 19 November 2026

Boss directed that the blanket HOLD be corrected. VOID contributes a default icon
through `contributes.languages[].icon`. VS Code/VSCodium resolve that icon with
the selected file-icon theme. Seeing VOID while Material Icon Theme is active is
expected when Material has no VOID mapping. It does not establish an override
regression. There is no need for a second theme, forced CSS or editor load-order
workaround in the VOID extension.

The [official language-extension documentation](https://code.visualstudio.com/api/language-extensions/overview#file-icon)
describes language icons as fallbacks subject to theme mappings and
`showLanguageModeIcons`. The actual
[VS Code 1.144.0 generator](https://github.com/microsoft/vscode/blob/2c33b50b16c472efe58357d1d2a8a5414bc23fbf/src/vs/workbench/services/themes/browser/fileIconThemeData.ts#L337-L460)
marks theme language IDs as covered and suppresses the language fallback for them.
It uses generated CSS selectors, not a general rule that extension activation
order determines precedence.

Both Material Icon Theme 5.40.0's
[file mappings](https://github.com/material-extensions/vscode-material-icon-theme/blob/v5.40.0/src/core/icons/fileIcons.ts)
and [language mappings](https://github.com/material-extensions/vscode-material-icon-theme/blob/v5.40.0/src/core/icons/languageIcons.ts)
have no VOID entry. The untouched original Material VSIX's generated icon theme
also has no `void` filename, extension or language mapping. Current upstream main
was checked separately and has no VOID entry in those mapping lists.

### What the old test actually did

The preserved `Void.Language-G69-evidence/workbench.mjs` manually sets:

```json
"material-icon-theme.files.associations": {
  "*.void": "json",
  "player.void": "python"
}
```

Consequently the original light-mode custom-extension observation was real; it
was not a test of unmodified Material Icon Theme. Its receipts, screenshots,
comparison script, assets and archive are retained unchanged at
`/home/shmellyorc/Documents/Void.Language-G69-evidence`. The original G69 worktree
and all six local versions are likewise retained unchanged. The correction does
not relabel that observation as a pass or claim that every custom extension-only
mapping wins in every editor version. It removes the blanket requirement to wait
for a change in editor/theme selector behavior before providing a normal default
language icon. An explicit `void` language mapping is the verified theme-owned
route; Material supports this through `material-icon-theme.languages.associations`.

### Current candidate

The exact original black/white PNG bytes are applied on top of G76. Only the
`void` language icon declaration, packaging allowlist and file-icon README section
are added to the existing extension; the G73 top-level extension icon and all G76
extension behavior remain intact. No extension/theme is installed into a user
profile, no theme settings are changed in an existing profile, and no compiler or
language-server source changes are needed.

The new desktop replay uses temporary development hosts and a separate extracted
copy of the original stock Material VSIX for each product. It contains no
`--install-extension` command. Cases include Seti and stock Material in both
appearances, explicit `void → json` language mappings, specific `player.void`
filename mappings, and disabled language-icon fallback. Exact final receipts,
checks and qualification are recorded below.

### Final current qualification

| Check | Result |
| --- | --- |
| Current extension build/typecheck/unit tests | **13/13 PASS**, no skipped tests |
| Current VSIX packaging | PASS; both G69 PNGs and existing G73 extension icon match source bytes |
| PNG dimensions/colors/transparency/matching alpha | PASS; exact original asset hashes retained |
| VS Code 1.144.0, native Linux | **9/9 scenarios PASS**, 72 rendered icons inspected |
| VSCodium 1.143.17232, native Linux | **9/9 scenarios PASS**, 72 rendered icons inspected |
| Original six G69 file versions | Checksums unchanged in retained worktree |
| Whitespace and staging | `git diff --check` PASS; nothing staged |

Each desktop checks four `.void` files in both Explorer and tabs. The nine
scenarios are Seti dark/light, unmodified Material dark/light, explicit theme
language mappings dark/light, specific filename mappings dark/light, and disabled
language-mode fallback. An explicit `void → json` theme language mapping takes
precedence in both appearances, independently of the VOID default images.

The first current unit-test invocation selected the obsolete default
`build/debug/voidls` and failed project-protocol negotiation. That failure remains
in `extension-test.log`. Repeating with the qualified G76 server through
`VOIDLS_TEST_SERVER` passed all 13 tests. No compiler or language-server changes
were made to accommodate the outdated test binary.

Current structured evidence is [correction.json](evidence/g69/correction.json).
Raw receipts, screenshots, scripts, initial failure, successful logs and the
uninstalled candidate VSIX remain at `/tmp/void-g69-correction-x0yde37y`.
Native Windows/macOS remain **UNQUALIFIED**; Linux results do not imply otherwise.

### Recommendation and exact changed-file inventory

**INTEGRATE recommendation for the normal default file-icon contribution.**
The original blanket G69 HOLD is superseded by Boss's correction; the genuine
custom extension-only light-mode observation remains documented above. There is
no separate milestone or requirement to modify upstream editors before using the
standard contribution. No source changes outside extension metadata, assets and
related documentation are included.

Current candidate on top of G76: **10 paths**, six modified and four new:

```text
docs/README.md
docs/WORKTREE_MANAGEMENT.md
docs/projects/G69_VOID_FILE_ICONS.md
docs/projects/G73_VSCODE_EXTENSION_ICON.md
docs/projects/evidence/g69/correction.json
editors/vscode/.vscodeignore
editors/vscode/README.md
editors/vscode/icons/void-dark.png
editors/vscode/icons/void-light.png
editors/vscode/package.json
```

No changes are staged or committed; nothing was pushed or published. Original
worktrees, recovery materials and evidence remain preserved. Existing user editor
settings and installed extensions were not changed by the qualification replay.
