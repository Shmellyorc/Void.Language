# raylib public parameter names and documentation

> Storage correction, 2026-11-21: the historical `/tmp` evidence locations in this report have been cleaned. Retained Markdown and original failure records now use the [verified migration inventory](/home/shmellyorc/Documents/Void.Language-archives/storage-correction-2026-11-21/MIGRATION_INVENTORY.md). Redundant generated outputs and disposable fixtures were deleted under the Boss’s updated retention instruction. Historical commands and measurements below are unchanged.

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


## G77 additive source companions — 19 November 2026

The [G77 implementation record](G77_FALLIBLE_RAYLIB_LOADERS.md) adds 34 documented
source-level resource/file companions. All 757 functions qualified above remain
byte-identical in the original source prefix. Current documentation coverage is
791 functions, 1,591 ordered parameters and 539 value returns, plus documented
error types/members. The pinned 600-function native inventory and generated ABI
outputs are unchanged. New checks preserve the existing 20 historical meaning
regressions and add four companion documentation/signature mutations. Current
results and qualifications belong to G77; earlier results above remain history.

## Historical numeric-unit naming and partial camera correction — 19 November 2026

**Superseded by the final public API cleanup below.** The compatibility spellings
and passing runs in this section describe commit `2ed2c0e1` only. Those public
spellings are now retired; this section and its evidence remain historical.

Follow-up to committed G77 `2bb57ba167b59e7015244f7c5b32515d49101d52`,
within the raylib API maintenance scope. No new milestone or compiler redesign.

The original CamelCase converter treated the `D` following a digit as a separate
word, yielding `camera3_d`, `begin_mode2_d` and `draw_triangle_strip3_d`.
Public naming now preserves the meaningful numeric-letter unit: `camera_3d`,
`begin_mode_2d`, `draw_triangle_strip_3d`. The
[package guide](../../verified-packages/raylib/README.md#camera-construction-and-numeric-unit-names)
lists all 14 preferred/compatible pairs. Trailing numbers without a following
letter, including `vector3`, `compute_sha256` and `load_utf8`, are unchanged.

Every original G77 public function remains available. New functions forward to
the original implementations, with the same parameter labels, order, types,
returns and ownership. Old `##` comments identify the preferred spelling; new
functions have actual descriptions, ordered parameter docs and applicable return
docs. Native camera records still carry their existing `camera2_d`/`camera3_d`
identities, and both constructor spellings return those identical value types.
No descriptor, native declaration, C wrapper, layout, resource owner or consuming
terminal changes. The generator's `abi_snake` retains native identifiers;
public `snake` groups numeric-letter units. `inventory.json` keeps `void_name`
stable and adds preferred `public_name` / `compatibility_aliases` metadata.

### Camera inconvenience and compatible correction

Before editing, a fully specified global `camera2_d` checked successfully.
`camera2_d(zoom = 2)` failed both locally and globally with `VOID2004`, missing
the required `offset` argument. That directly identifies the helper's required
parameters, rather than a top-level initialization restriction.

Both old and preferred camera helpers now accept pure named/default arguments.
2D defaults are zero offset/target/rotation and zoom 1. 3D defaults are position
`(0, 0, 1)`, target zero, up `(0, 1, 0)`, fovy 45 and perspective projection.
These produce an ordinary nondegenerate initial view; explicit custom values
retain their original meaning and remain the caller's responsibility. Supplying
every field keeps the previous behavior. No window/native allocation is involved
in these value constructors. For example:

```void
use pkg/raylib as rl

var camera = rl.camera_3d(position = rl.vector3(0, 2, 10))
var overlay = rl.camera_2d(zoom = 2)

func main():
    var local = rl.camera_3d(fovy = 60)
    print(local.fovy)
```

The consumer examples use preferred names and omit the unchanged camera fields.
The adjustment uses existing named/default and pure aggregate construction;
no parser, semantic, global-initialization or backend change was needed.

### Bounded upstream comparison

[Rust's API naming guidance](https://rust-lang.github.io/api-guidelines/naming.html)
treats compound abbreviations as units. VOID retains its own existing lowercase
conventions and repairs only the affected numeric-letter split. Pinned raylib
6.0 [2D camera](https://github.com/raysan5/raylib/blob/6.0/examples/core/core_2d_camera.c)
and [3D camera](https://github.com/raysan5/raylib/blob/6.0/examples/core/core_3d_camera_free.c)
examples use ordinary value construction followed by selected fields, with 2D
zoom 1 and 3D Y-up, 45-degree perspective. Those released examples inform the
helper defaults; they are not an upstream patch or proof of a universal camera
configuration. VOID uses a minimal nonzero default camera position.

### Focused verification and limits

Current source documentation covers **805 functions, 1,624 ordered parameters
and 544 value returns**: the 791 G77 functions plus 14 preferred spellings.
The maintained documentation check retains all 20 historical and four G77
mutations, adds eight naming cases and three compatibility mutations, and checks
all 14 aliases. The native naming test compares old/new camera transforms and
plain ABI fields, global/local partial constructors, full positional calls,
all preferred/old drawing names, and rejection of an unknown camera label.
Compiler/LSP checks cover namespace completion of both spellings, actual source
hover docs, displayed optional parameters and active named-parameter selection.

Final results and exact changed-file inventory are recorded in
[evidence/raylib-naming/results.json](evidence/raylib-naming/results.json).
Focused results on the final code:

| Check | Result |
| --- | --- |
| Release compiler, headless + real graphics, JIT/AOT O0/O2 | 15/15 commands PASS |
| Debug compiler, same source and modes | 15/15 commands PASS |
| Fatal ASan/UBSan/leaks headless, JIT/AOT O0/O2 | 11/11 commands PASS |
| Documentation/native named-call/LSP regressions | PASS with Release, Debug and strict tools |
| Actual VS Code 1.144.0 | 12/12 checks PASS, including visible optional-camera parameter help |
| Registered source documentation test | 1/1 PASS |
| Updated consumer examples | Source check PASS |
| Actual regeneration and subsequent complete inventory check | PASS; native files byte-identical |

No compiler, Runtime, LLVM, extension implementation or optimization changes
were made. The additional public forwarders and defaults were not independently
throughput-benchmarked; this correction makes no new performance claim. Tests
use the already-qualified LLVM 23.1.4 binaries and explicit local native catalogs.
No rebuild or unrelated full compiler suite was needed. Native-value return type
labels retain `camera2_d` / `camera3_d`; preferred callable names are corrected
without inventing new nominal types or changing native identity. VSCodium was
not rerun for this correction.

Original pre-correction diagnostics and all unsuccessful intermediate evidence
remain under `/tmp/raylib-naming-o9tvv7ve`. The first added LSP assertion looked
for a top-level SignatureInformation documentation field; VOID publishes the
operation description through hover and individual parameter docs through
signature help. That test harness assumption was corrected without altering
the server or weakening the documentation checks. An intermediate hover probe
used an unfinished call; the final check uses a valid constructor call. The first
expanded real-editor test also inherited a Result-return assertion for camera
values; it now verifies the actual native camera return types. All these failed
harness runs remain in the raw evidence.

Native Windows/macOS remain unqualified. Strict graphics/audio device leak
failures from G77 remain preserved and are not claimed as passes here. Existing
stack limits, fatal sanitizer policy, native approvals and installed tools are
unchanged. No staging, commit, push, installation or hosted CI is authorized.


## Final public API cleanup — Boss-directed correction

Baseline: `2ed2c0e101e1c503fdddce8bd8f6564caf506593`.
The Boss superseded the prior compatibility instruction and authorized immediate
commit/push with source/Git consistency checks only. **Tests, editor runs,
benchmarks and independent review were not rerun at Boss direction.** Earlier
15/15, 11/11 and 12/12 records above are historical and do not qualify this change.

All 14 corrected names are now the only public spellings. The duplicate
forwarding functions were removed and the original implementations renamed
in place. The native calls and nominal record identities are unchanged.
Generator metadata retains native `void_name`, publishes canonical `public_name`
and removes `compatibility_aliases`. Source verification and maintained
IntelliSense expectations reject retired public names instead of requiring them.
The public module now has 791 functions, with one constructor per value helper.

Partial construction extends beyond cameras. Vectors default to zero, matrices
to identity, colors to opaque black, rectangles to empty values, transforms to
zero translation / identity quaternion / unit scale, rays to origin / unit -Z,
ray collisions to no-hit/zero payload and bounding boxes to zero-sized origin
boxes. Patch source rectangles remain required, with zero borders and nine-patch
layout defaults. Bone names remain required, with parent -1. VR device/stereo
calibration and automation frame/type/payload remain explicit because there is
no universal valid calibration or event-specific payload. Camera defaults remain
as requested. The package guide and each physical `##` description document the
choices; supplied arguments keep their existing meanings and validity obligations.
All defaults use existing pure value construction and default arguments, for
both globals and locals. No compiler redesign or duplicate constructor was added.

Examples, public API references, source-documentation checks and editor/native
fixtures now target canonical public names and broader partial value construction.
Those maintained checks were updated but **not executed**. Native C, descriptors,
layouts, declarations, resource ownership and installed tools remain unchanged.
Source checks cover Python/JavaScript syntax, declaration/name/default consistency,
metadata agreement, unchanged native bytes, intended-file inventory and Git
whitespace. No compiler build, regression suite, benchmark or hosted CI was run.
Windows/macOS remain unqualified; the original strict graphics/audio failures
and all prior unsuccessful evidence remain preserved. No tools were installed
and no backups were created. Integration stops after the authorized normal push.


Exact final-cleanup changed-file inventory (14 files):

```text
docs/README.md
docs/projects/RAYLIB_PUBLIC_API_DOCUMENTATION.md
examples/raylib-consumer/README.md
examples/raylib-consumer/src/main.void
tests/project/raylib_documentation.py
tests/project/raylib_fallible_editor.cjs
tests/project/raylib_naming.py
tools/raylib/README.md
tools/raylib/generate.py
tools/raylib/inventory.json
tools/raylib/names.py
tools/raylib/public_docs.py
verified-packages/raylib/README.md
verified-packages/raylib/src/raylib.void
```
