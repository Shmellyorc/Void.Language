# raylib public parameter names and documentation

Source parent `7ef5ba6d8354ba295ec0a3d63c320b259cfd326b` (G72).
Work is isolated on `codex/raylib-public-api-docs`. G73's four-file icon
candidate and the owner's installed extension retain their prior hashes.

## Public source changes

The pinned `verified-packages/raylib/upstream/raylib.h` and verified native
adapters supply parameter names, argument ordering, output fields and ownership
contracts. The header SHA-256 is
`047e7255f93f8c34039cab906ad76136706b5c7b4c5b5b065d84141963ee9b6b`.
Clang 23.1.4 provides the declarations.

All 757 public functions now have attached Section 46 descriptions.
Their 1,524 parameters have ordered `## @param` tags, and all 505
value-returning functions have `## @return` tags. This covers the 600
raylib interfaces and 157 source helpers. Parameter labels change on 577
functions, including the 465 wrappers that formerly used generated positional
labels. Wrapper bodies use the renamed parameters and disambiguated locals.

The public signature for drawing a circle is

```void
## Draw a color-filled circle.
## @param center_x Horizontal coordinate of the circle center in pixels.
## @param center_y Vertical coordinate of the circle center in pixels.
## @param radius Circle radius in pixels.
## @param color_value Color including its alpha channel.
func draw_circle(center_x: i32, center_y: i32, radius: f32, color_value: native.color):
```

VOID rejects shadowing of visible declarations. The requested literal
`color` parameter conflicts with the existing `color()` constructor and
produces `VOID2005`. Parameters whose snake_case name collides with a public
type or callable receive `_value`, such as `color_value` and
`image_value`. The reserved word `event` becomes `event_value`.
These exceptions preserve the current language and constructor contracts.

Fourteen pinned functions themselves use `p1` through `p4` for spline,
line, triangle and quad points. Those original labels remain, with their
declaration order documented. The header also leaves short mathematical
parameters such as `t` and mesh `size` without complete role or unit
descriptions. Their documentation retains that limited meaning.

The manual adapters were reviewed alongside their implementations. Docs explain
the previous-codepoint byte cursor, consumed resource terminals, callback
registrations, font-atlas rectangles, decoded codepoint value/size fields,
counted UTF-8 results, static-result copies, nullable shader strings,
parent-retained array views and transferred mesh/texture ownership.
The original scalar types, returns, visibility, resource metadata and native
ABI identities are preserved.

## Documentation accuracy follow-up

Review found that the one-time documentation templates inferred too much from
names alone. An `_close` suffix incorrectly treated `window_should_close` and
`gallery_should_close` as consuming terminals. Shared labels such as `filter`,
`key`, `spacing`, `length`, `replace` and `c2` crossed unrelated API domains.
Some inherited C summaries also described static storage even though the VOID
adapter returns an owning copy. Numeric-return boilerplate obscured the result.

The follow-up corrects documentation on 261 functions against the pinned C
declarations, physical VOID bodies, native adapters and ownership contracts.
It covers close-condition booleans, texture filtering, image font color keys,
grid and line spacing, gradient angles, vertex colors and Bezier controls,
pixel/world distances, generated counts, lexical filename queries, text byte
offsets, audio seek time,
copied static results, parent-retained resources and exclusive transfers.
It also removes incorrect grammar and replaces vague numeric return text.

The public source with all `##` lines removed is byte-identical to the reviewed
patch. Parameter names, types, bodies, binding behavior, ABI identities and
ownership metadata are unchanged. The compiler completion implementation and
its tests retain their reviewed hashes. No installed tool is replaced.

The existing generator preserves the corrected physical documentation. Its
documentation check now rejects known misleading descriptions. A regression
checks all 757 functions and injects 20 independent historical errors in memory,
requiring each to fail. Protocol tests also verify the corrected window query
hover and texture-filter, font-key, grid-spacing, line-spacing and vertex-color
parameter descriptions.

These are targeted checks, not proof of prose correctness. Future changes still
need comparison with the pinned declarations and adapters. Where those sources
do not specify a meaning, the docs retain that limit. In particular, the pinned
header does not specify the unit of `set_audio_stream_buffer_size_default.size`
or the failure sentinel of `file_text_find_index`; no meaning is invented.

Follow-up verification reran regeneration, `--check --require-complete`, the
documentation-only checks and the physical-raylib Debug/Release JIT/AOT and LSP
regression using both Release and fatal-sanitizer builds. Release again passed
48/48 and fatal ASan/UBSan/leak detection passed 15/15. The native inventory and
generated binding outputs remain byte-identical to G72. Logs and the exact
incremental review patch are in the evidence directory's `doc-quality/` folder.

## Generic named-argument completion repair

G72's ordinary function-call probe published signatures and parameter docs
but supplied no unused parameter labels. Its active-argument search only
visited argument expressions, so it also missed a cursor inside an existing
named label. Named calls still compiled. A baseline regression against the
installed G72 server reproduced the missing labels with a small local function.

Following explicit approval to repair this generic gap, the existing callable
probe now uses `argument_mapping` to bind all other written arguments while
skipping the cursor. Unbound parameters become compiler-owned completion
items with their existing types and documentation. Editing a label preserves
its existing equals sign. A positional expression still receives normal
expression completion. Nested calls use the inner callable's metadata.

Generic declaration probes use the same pattern-type formatter as signature
help. The first repair attempted to print a generic array before its element
type was resolved. Existing binary and collections signature-help tests caught
that error. The final implementation uses `editor_pattern_type` and adds a
generic-array regression. Enum/result constructors retain their existing
argument rules.

## Regeneration and compatibility

The native generator continues to preserve the physical VOID source.
A small `public_docs.py` check verifies C-derived labels, explicit adapted
signatures, descriptions, ordered parameter tags and applicable return tags.
Both ordinary generation and strict checking execute that validation.

```sh
python3 tools/raylib/generate.py --check --require-complete --clang /path/to/clang
```

Strict verification still accounts for all 600 interfaces. The inventory,
native C files, descriptor inputs, native VOID declarations and upstream
files match G72 byte for byte.

An independent audit reverses the parameter and local renames, removes docs
and compares the entire public source with G72. It confirms identical
function types, return types, ordering and implementation bytes after those
renames. The normalized source SHA-256 is
`03f33325e732cbd9f8db20ee3089613288ef7f387576ce9ed9596454cdd50b53`.

The caller audit found 444 checked-in raylib calls in examples, tests and
editor fixtures, with no stale named labels. Those existing calls are
positional. The existing named `c_string(value = ...)` construction uses
the same field metadata as before.

External callers using old labels need a source update. For example,
`get_random_value(p0 = 1, p1 = 6)` becomes
`get_random_value(min = 1, max = 6)`. The named-label regression confirms
that an old `p0` label is rejected. The complete old/new parameter map is
in the evidence directory's `renames.json`. Existing copied packages and
their locks retain their installed source until deliberately updated.

## Verification

The focused source and protocol regressions exercise local, imported,
receiver and explicitly generic calls, including generic arrays. Cases cover
partial labels, existing equals signs, positional and named arguments already
supplied, later supplied labels, invalid bindings, nested calls, retained
expression completion and repeated edits with semantic recovery.

The physical-raylib regression checks named calls in Debug and Release
JIT/AOT execution, hover, active-parameter descriptions, named completion,
old-label rejection and clean recovery.

Real Linux VS Code 1.144.0 loaded the installed G73 extension from
`/home/shmellyorc/.vscode/extensions/void-language.void-language-0.1.2`
and the candidate server in an isolated profile. The visible suggestion
widget displayed `center_x =` and `center_y =` for
`rl.draw_circle(cen`. Accepting a suggestion inserted the label once.
The parameter popup showed the center coordinate description. Local,
generic and raylib calls excluded supplied labels.

The same source project and extension were exercised with the installed G72
server and candidate server. The replay confirms a semantic token for the
new generic declaration before measuring edits to its caller. Three dot-widget
trials had medians of 175 ms and 169 ms respectively. Candidate named
completion took 135 ms. Each dot trial retained 62 semantic tokens, and
correction cleared the diagnostics. These are small functional samples.
Screenshots and running process paths are retained in the desktop receipts.

Final Release passed 48/48. Fatal ASan, UBSan and leak detection passed
15/15 focused tests, including the existing binary and collections tests.
Both builds also passed the separate physical-raylib documentation regression
in Debug and Release JIT/AOT modes.
Linux headless physical-package, callback and buffer-guard suites passed and cover
JIT/AOT, lock checks, native admission, callback replacement/retirement,
failure cleanup and relocated execution.

Windows and VSCodium were outside this run. Full graphics/audio sanitizer
qualification was outside these headless checks. The previously documented
device leaks remain unsuppressed. Public GitHub acquisition and production
release trust retain their existing qualification boundaries.

## Review inventory and installation state

- `CMakeLists.txt`
- `src/frontend/compiler.cpp`
- `tools/raylib/generate.py`
- `tools/raylib/public_docs.py`
- `tools/raylib/README.md`
- `verified-packages/raylib/src/raylib.void`
- `verified-packages/raylib/README.md`
- `tests/lsp/named_arguments.py`
- `tests/project/raylib_documentation.py`
- `docs/projects/RAYLIB_PUBLIC_API_DOCUMENTATION.md`

Review builds, native-catalog copies, test projects, logs, screenshots and
receipts are in
`/home/shmellyorc/Documents/Void.Language-RaylibDocs-evidence`.
The owner's G72 compiler/server links, VS Code settings and recovery
installations passed preservation checks. Candidate tools were used in
isolated processes. G69 remains on HOLD.

All ten files are unstaged for review on the branch above.
