# G88 — Standard library and verified packages modernization

20 November 2026. **Corrected engineering candidate for focused Golden Wrench re-review.**
Accepted baseline: `9b7bbff41ba9a49ebc3d35a01fe77a7b5103ee77` (G87).
The former G91 scope is included here; no separate G91 milestone is created.
Everything remains unstaged. No commit, branch, push, installation, publication or
hosted CI is authorized or performed. Root README and vendored code are unchanged.

## Scope and audit method

Inventory every file under `stdlib/` and `verified-packages/`, then inspect source
representations, native contracts, public docs and maintained consumers. The
baseline inventory records exact bytes, SHA-256, line counts and declarations in
`baseline-inventory.json` under the evidence root. There are 29 standard source
modules and two raylib source modules. Supporting native, provider, preparation,
manifest, documentation and upstream files are included below. Public source docs
were checked for every top-level public declaration; the missing network/process
descriptions were supplied. No new frontend, Runtime, scheduler or enum value
translation framework is introduced.

## Maintained package-preparation inventory

`verified-packages/raylib-prepare.void` is the maintained Python-free raylib
preparation program added by [G94](G94_VOIDUP_PYTHON_FREE_DEVELOPER_EXPERIENCE.md).
It is a required package resource, not a generated receipt or obsolete snapshot.

## Complete standard-library inventory

Every row records an actual source inspection and a decision, including deliberate
retention where modernization would not improve the implementation. Baseline line
counts provide a reproducible inventory independent of subsequent edits.

| Source | Baseline lines | Opportunity and disposition |
| --- | ---: | --- |
| `stdlib/archive.void` | 104 | Already checked owned ZIP readers/results and explicit cleanup. Retain ZIP signatures, limits, sizes and entry EOF/directory fields as protocol data; a generic iterator must not skip integrity draining. |
| `stdlib/channel.void` | 100 | Already generic router/state/topic types, nullable registrations and event snapshots. Retain opaque identity subscriptions, exact unsubscribe and no finalizer semantics; no new iteration/async facade is justified. |
| `stdlib/collections.void` | 361 | Already generic stable sorting/search/map/fold and typed results. Retain comparator callbacks, independent outer copies and live invalidation. Existing generic equality already benefits from G83 without a rewrite. |
| `stdlib/console.void` | 640 | Replace native color/key/error conversion chains with explicit numeric enums and checked casts; private capability/key-metadata flags replace arithmetic bit tests. Retain public Boolean snapshots, repeat counts, endpoint IDs and resource sessions. |
| `stdlib/convert.void` | 64 | Already generic checked parsing and result helpers. Retain two tagged errors: the branches also select distinct messages/input evidence, not a duplicated ordinal table. unwrap and unwrap_or intentionally have different failure/evaluation behavior. |
| `stdlib/crypto.void` | 68 | Already checked managed SHA-256 owner and signature verifier. Retain key/signature lengths, native replies, explicit finish/close and Runtime ledger cleanup; no unsafe fallible-constructor substitution. |
| `stdlib/encoding/ascii.void` | 140 | Already typed results and bounded strict ASCII. Retain character ranges, the output budget and tagged failures; these integers encode character data and quantities. |
| `stdlib/encoding/base64.void` | 240 | Replace arithmetic field extraction/assembly with integer masks/shifts. Retain alphabet/padding tagged enums, canonical trailing-bit rejection, offsets, bounds and RFC alphabet bytes. |
| `stdlib/encoding/binary.void` | 74 | Replace division/remainder/multiplication byte packing with explicit u64 masks/shifts. Retain generic exact-width/IEEE bit intrinsics, byte_order tagged enum, independent bytes and exact-width failures. |
| `stdlib/encoding/hex.void` | 115 | Replace arithmetic nibble extraction/assembly with shifts/masks. Retain hex digit grammar, character-code ranges, byte offsets and output limits. |
| `stdlib/error.void` | 9 | Already the documented common managed diagnostic base. Retain message/path/code; native and module codes are diagnostic numbers, not one universal enum. |
| `stdlib/io/memory.void` | 275 | Already owned cursor state, generic numeric methods and explicit results. Retain factories and transactional state changes; custom iteration would add cursor/draining semantics rather than simplify these methods. |
| `stdlib/io.void` | 1935 | Explicit native numeric error_kind/open_mode/seek_origin; checked unknown-code fallback; remove redundant one-way mapping helpers. Retain entry_kind and text_read tagged shapes, native reply layouts, Boolean adapters, progress and private-directory authority. |
| `stdlib/json.void` | 1142 | Explicit stable numeric error_kind eliminates the ordinal-code mapping; private parser phases become named enum states. Migrate formatted comparisons to direct enum equality. Retain tagged payload/value_kind, iterative frames, exact number tokens, work counters and grammar bytes. |
| `stdlib/log.void` | 1301 | Explicit numeric severity ordering removes the rank table. Retain uppercase framing-label mapping, which intentionally differs from member names; retain tagged sink/adoption/failure types and guarded resource/rotation state. |
| `stdlib/math.void` | 512 | Already generic kernels and numeric prelude delegation. Retain mathematical constants, IEEE special-case paths, signed-zero/NaN behavior and meaningful wrapper documentation; static members add no needed state here. |
| `stdlib/net/address.void` | 116 | Remove manual native-family number selection in the existing internal adapter; retain exact 0/4/6 public values. Complete public member/parameter/result documentation. Keep host-order ports, scopes and DNS budgets numeric. |
| `stdlib/net/error.void` | 73 | Already exact u8 native enums and checked unknown-code fallback. Add category/member/domain and complete error-field docs, including progress/consumption/cleanup evidence. Retain native status numbers and error phase identity. |
| `stdlib/net/http.void` | 1387 | Explicit numeric policy/error/phase/version enums remove repeated source/native mapping chains; reuse checked IO/URL enum conversions. Private feature flags replace arithmetic masks. Retain tagged credentials/body/trust alternatives, primitive replies, status codes, finite budgets and acquisition leases. |
| `stdlib/net/tcp.void` | 215 | Complete parameter/result and scoped-cleanup docs. Retain owned streams/listeners, independent accepted lifetimes, async external completion, one reader/one writer, deadlines and Boolean native adapters. |
| `stdlib/net/udp.void` | 103 | Complete parameter/result and cleanup docs, including discarded oversized datagrams and local-send semantics. Retain unicast/family limits, zero-size datagrams, async completion and shared close authority. |
| `stdlib/os.void` | 365 | Explicit native numeric error_kind and checked decode replace the category chain. Retain descriptions, platform-specific path policies, exact native domains/status, absent-versus-empty environment and application identity rules. |
| `stdlib/path.void` | 106 | Already documented checked host-lexical wrappers using IO authority. Retain these public convenience functions; deleting them would expose private host intrinsics rather than remove redundant public behavior. |
| `stdlib/process.void` | 144 | Decode existing capture_end through as? with the original unknown-code fallback; document every outcome, capture parameter and result evidence field. Retain literal native launch authority, bounded byte prefixes, exit/signal integers and direct-child reap/drain. |
| `stdlib/random.void` | 1098 | Replace private XOR intrinsic calls and arithmetic rotate/shift workarounds with source integer operators. Preserve version-1 SplitMix64/xoshiro256** constants, state copy/authority, draw consumption and generic sampling. Keep per-primitive arity adapters: they implement the current compiler-owned numeric facade, not ordinary overloads. |
| `stdlib/system_error.void` | 73 | Explicit sparse host error discriminants replace the manual mapping chain; checked unknown-code fallback preserves the original raw error.code. Retain shared diagnostic type and operation/native evidence. |
| `stdlib/text.void` | 577 | Already checked bounded Unicode/literal operations and generic-free clear APIs. Retain UTF-8/Unicode tables, scalar/byte boundaries, KMP/work bounds and character quantities. Small tagged error branches preserve distinct argument semantics. |
| `stdlib/time.void` | 808 | Already validated factories/results, private constructors, readonly components, local-choice enum and native timers. Retain full-range arithmetic, timezone/format contracts and clock contexts; converting native acquisitions/validation to fallible init would require a separate readiness/ownership change. |
| `stdlib/url.void` | 158 | Explicit native error_kind values and checked fallback remove category selection branches. Complete parameter docs. Retain owned immutable addresses, query presence booleans, HTTP(S) restrictions, percent/UTF-8 validation and shared CURLU parsing. |

`stdlib/README.md` was also audited and now explains numeric enum identity,
explicit `.name`/`.value`, checked conversion and migration, with links to the
individual API/resource contracts. Its runnable binary-save example remains valid.

## Complete verified-package inventory

| File | Audit outcome |
| --- | --- |
| `verified-packages/README.md` | Existing explicit verified-source/native-admission contract retained; one package, raylib, is present. |
| `verified-packages/raylib/.gitattributes` | Line-ending policy retained. |
| `verified-packages/raylib/.gitignore` | Generated/native artifact exclusions retained. |
| `verified-packages/raylib/OWNERSHIP.md` | Audited all owner/borrow/callback contracts; unchanged. No callback lifetime, parent lease, close, GC authority or acquisition semantics changed. |
| `verified-packages/raylib/README.md` | Add enum/flags migration, widths and explicit native conversion rules; distinguish raw native records, event results, callback levels and runtime indices. |
| `verified-packages/raylib/native/raylib.descriptor.in` | Native identity, ABI 3 and source-file admission remain byte-identical. |
| `verified-packages/raylib/native/raylib_api.c` | Generated C forwarding wrappers remain byte-identical. |
| `verified-packages/raylib/native/raylib_api.descriptor.inc` | Generated primitive binding/resource descriptors remain byte-identical. |
| `verified-packages/raylib/native/raylib_api.h` | Generated prototypes remain byte-identical. |
| `verified-packages/raylib/native/raylib_callbacks.inc` | Context-first adapters and unregister/drain contracts remain byte-identical. |
| `verified-packages/raylib/native/raylib_parity.h` | All existing layout/prototype/enumerator assertions remain byte-identical and still compile against the pinned header. |
| `verified-packages/raylib/native/raylib_provider.c` | Ownership ledger, callbacks, sealing and provider operations remain byte-identical. |
| `verified-packages/raylib/native/raylib_provider.h` | Provider ABI and primitive types remain byte-identical. |
| `verified-packages/raylib/src/raylib.void` | Replace all 305 flat enum/flag constants with 21 documented types. Type 43 public selector signatures including fallible companions; retain seven explicit raw keyboard-code companions and extract primitive values at native boundaries. Retain resource wrappers and their native cleanup behavior. |
| `verified-packages/raylib/src/raylib_native.void` | All 1,699 lines unchanged. Host-admitted primitive declarations/record fields remain exact; this is the native boundary, not an independent public convenience API. |
| `verified-packages/raylib/upstream/LICENSE` | Upstream provenance/license unchanged. |
| `verified-packages/raylib/upstream/raylib.h` | Pinned raylib 6.0 header unchanged; SHA-256 047e7255f93f8c34039cab906ad76136706b5c7b4c5b5b065d84141963ee9b6b. Authoritative discriminants/prototypes, not an assumed generic C enum representation. |
| `verified-packages/raylib/void.project.toml` | Package identity, version and entry remain unchanged; no new provider/consumer feature conditions are needed. |
| `verified-packages/raylib-prepare.py` | Pinned input hashes, descriptor generation, native compiler flags, sealed-owner provider build and catalog receipt/publication checks retained. No provider preparation or installation policy is relaxed. |
| `verified-packages/raylib.package.cmake` | Existing source baseline/snapshot identities retained; configured builds refresh the ordinary package snapshot. |

## Bounded upstream research

The [raylib quick reference](https://www.raylib.com/cheatsheet/cheatsheet.html)
confirms the API families, but currently labels itself v6.2 and is not the numeric
oracle for this pinned 6.0 package. The checked-in, hash-verified 6.0 header and
Clang parity assertions govern all values and boundary prototypes.
The authors' [xoshiro256** source](https://prng.di.unimi.it/xoshiro256starstar.c)
and [SplitMix source](https://prng.di.unimi.it/splitmix.c) supply the established
shift/XOR/rotation form used to remove arithmetic workarounds. Only the existing
version-1 transition/seed operations are retained; no new upstream jump feature,
algorithm version or vendor patch is imported. The existing independent Python
reference/draw-consumption tests remain the behavioral oracle.

## Enum and flags API

The existing Clang inventory remains the single provider-value oracle.
`tools/raylib/enums.py` supplies only 21 type/prefix/width choices and verifies
physical source discriminants and docs against that oracle; it does not duplicate
305 numeric values. `generate.py --check --require-complete` verifies generated
native outputs and the typed source; `inventory.json` now names the real enum
members. C enum storage is never presumed equivalent to a VOID enum.

| Provider category | Public type | Backing |
| --- | --- | --- |
| ConfigFlags | `config_flags` | `u32` flags |
| TraceLogLevel | `trace_log_level` | `i32` |
| KeyboardKey | `keyboard_key` | `i32` |
| MouseButton | `mouse_button` | `i32` |
| MouseCursor | `mouse_cursor` | `i32` |
| GamepadButton | `gamepad_button` | `i32` |
| GamepadAxis | `gamepad_axis` | `i32` |
| MaterialMapIndex | `material_map_index` | `i32` |
| ShaderLocationIndex | `shader_location_index` | `i32` |
| ShaderUniformDataType | `shader_uniform_type` | `i32` |
| ShaderAttributeDataType | `shader_attribute_type` | `i32` |
| PixelFormat | `pixel_format` | `i32` |
| TextureFilter | `texture_filter` | `i32` |
| TextureWrap | `texture_wrap` | `i32` |
| CubemapLayout | `cubemap_layout` | `i32` |
| FontType | `font_type` | `i32` |
| BlendMode | `blend_mode` | `i32` |
| Gesture | `gesture_flags` | `u32` flags |
| CameraMode | `camera_mode` | `i32` |
| CameraProjection | `camera_projection` | `i32` |
| NPatchLayout | `n_patch_layout` | `i32` |

The native-facing selector families are window/config masks, logging, keyboard,
mouse/gamepad controls, gestures, camera/blending/patch layout, image/texture
formats and sampling, material-map selection, uniform type and font-data type.
Public library comments and headings describe APIs; milestone history stays in
engineering documentation. The pre-existing fallible-loader milestone marker was
replaced with a descriptive section comment, and its test anchor was updated.
Named selector arguments use their nominal types. Seven raw keyboard-code
companions preserve the provider domain beyond named enum cases. Raw native calls/records use
`.value`; no change reaches their descriptors or binary ABI. Config/gesture masks
preserve unknown u32 bits, and ordinary enum conversions validate declared members.

### Exact selector signature migration

Parameter names and order remain stable; only these public selector types change.
The matching primitive extraction is explicit in each wrapper, while `try_`
companions forward the typed value to their source-level loader.

| Public function | Typed parameter |
| --- | --- |
| `begin_blend_mode` | `mode: blend_mode` |
| `camera_3d` | `projection: camera_projection` |
| `clear_window_state` | `flags: config_flags` |
| `get_gamepad_axis_movement` | `axis: gamepad_axis` |
| `get_key_name` | `key: keyboard_key` |
| `get_pixel_color` | `format: pixel_format` |
| `get_pixel_data_size` | `format: pixel_format` |
| `image_format` | `new_format: pixel_format` |
| `is_gamepad_button_down` | `button: gamepad_button` |
| `is_gamepad_button_pressed` | `button: gamepad_button` |
| `is_gamepad_button_released` | `button: gamepad_button` |
| `is_gamepad_button_up` | `button: gamepad_button` |
| `is_gesture_detected` | `gesture: gesture_flags` |
| `is_key_down` | `key: keyboard_key` |
| `is_key_pressed` | `key: keyboard_key` |
| `is_key_pressed_repeat` | `key: keyboard_key` |
| `is_key_released` | `key: keyboard_key` |
| `is_key_up` | `key: keyboard_key` |
| `is_mouse_button_down` | `button: mouse_button` |
| `is_mouse_button_pressed` | `button: mouse_button` |
| `is_mouse_button_released` | `button: mouse_button` |
| `is_mouse_button_up` | `button: mouse_button` |
| `is_window_state` | `flag: config_flags` |
| `load_font_data` | `type: font_type` |
| `load_image_raw` | `format: pixel_format` |
| `load_texture_cubemap` | `layout: cubemap_layout` |
| `n_patch_info` | `layout: n_patch_layout` |
| `set_config_flags` | `flags: config_flags` |
| `set_exit_key` | `key: keyboard_key` |
| `set_gestures_enabled` | `flags: gesture_flags` |
| `set_material_texture` | `map_type: material_map_index` |
| `set_mouse_cursor` | `cursor: mouse_cursor` |
| `set_pixel_color` | `format: pixel_format` |
| `set_shader_value` | `uniform_type: shader_uniform_type` |
| `set_shader_value_v` | `uniform_type: shader_uniform_type` |
| `set_texture_filter` | `filter: texture_filter` |
| `set_texture_wrap` | `wrap: texture_wrap` |
| `set_trace_log_level` | `log_level: trace_log_level` |
| `set_window_state` | `flags: config_flags` |
| `trace_log` | `log_level: trace_log_level` |
| `try_load_image_raw` | `format: pixel_format` |
| `try_load_texture_cubemap` | `layout: cubemap_layout` |
| `update_camera` | `mode: camera_mode` |

### Raw keyboard input

`KeyboardKey` is a closed set of named header values, while raylib/GLFW accepts
additional native input codes. Named calls such as
`is_key_down(keyboard_key.escape)` remain unchanged. The seven explicit
`is_key_pressed_code`, `is_key_pressed_repeat_code`, `is_key_down_code`,
`is_key_released_code`, `is_key_up_code`, `set_exit_key_code` and
`get_key_name_code` companions accept raw `i32` and call the same native
operations. Named wrappers delegate with `.value`; no mapping chain, new enum
case, implicit conversion or descriptor/ABI entry is introduced.

This follows the existing VOID contract: numeric enums are closed and nominal,
and ordinary function overloads are unavailable. Treating key codes as flags
would invent inappropriate bitwise semantics; a generic integer conversion would
also accept unrelated numeric enums. Explicit raw operations preserve both exact
named-key binding and the open native domain. No compiler feature is added.

Pinned upstream [GLFW key definitions](https://raw.githubusercontent.com/raysan5/raylib/6.0/src/external/glfw/include/GLFW/glfw3.h)
define F13=302 through F25=314. The [GLFW callback](https://raw.githubusercontent.com/raysan5/raylib/6.0/src/platforms/rcore_desktop_glfw.c)
stores/queues these raw values, and [raylib state queries](https://raw.githubusercontent.com/raysan5/raylib/6.0/src/rcore.c)
accept positive indices below 512. `get_key_pressed()` results therefore go directly
to `*_code` operations without a closed enum cast. Raw codes pass unchanged;
invalid values retain native behavior (all five pinned state queries return false
for <=0 or >=512). `set_exit_key_code(0)` disables the exit key. Naming may return
null and requires an initialized backend; returned buffers retain their close
contract. Actual support remains backend-specific. The public package README and
source `##` documentation describe both paths.

### Migration

```void
use pkg/raylib as rl

func main():
    var mask = rl.config_flags.window_hidden | rl.config_flags.window_resizable
    rl.set_config_flags(mask)
    rl.set_exit_key(rl.keyboard_key.escape)
    var camera = rl.camera_3d(projection = rl.camera_projection.orthographic)
    print(camera.projection == rl.camera_projection.orthographic.value)
```

- Flat raylib constants move under their type with the common prefix removed:
  `flag_window_hidden` becomes `config_flags.window_hidden`, `log_warning` becomes
  `trace_log_level.warning`, `key_right` becomes `keyboard_key.right`,
  `pixelformat_uncompressed_r8g8b8a8` becomes
  `pixel_format.uncompressed_r8g8b8a8`. `key_null` becomes `keyboard_key.none`.
  Flat compatibility aliases are removed. Maintained examples and embedded test
  sources are migrated. Width casts previously needed for numeric masks are removed.
- Standard-library enum member names and ordinary selection syntax remain stable.
  Their explicit numeric values now encode their existing native/error/priority
  contracts. Use `.name` for diagnostic text and `.value` for exact integers.
  Numeric enum interpolation is not currently a built-in display implementation.
  Compare enum values directly, including generic result/error tests.
- Native camera/patch fields, image format observations, callback log levels and
  raw event queries remain integers. Compare a native field to `member.value`;
  validate an incoming event with `as?` when closed-enum interpretation is desired.
- These source type/layout changes require rebuilding source consumers. There is
  no claim that existing compiled VOID enum-containing aggregates remain binary
  compatible; actual C ABI/record/symbol contracts are unchanged.

## Intentionally retained representations

| Representation | Reason |
| --- | --- |
| Native replies and C records | Exact primitive layouts/widths/signedness, native symbol identity and same-build bridge authority. |
| Native errno/Win32/Winsock/c-ares/CURL diagnostics | Open external numeric domains; preserve their raw codes, including unknown values. |
| HTTP statuses, ports, IP scope indices, sizes, byte/scalar offsets, EOF metadata, clock contexts and algorithm versions | Protocol identifiers, quantities or separately defined presence/state, rather than closed enum choices. |
| Raylib raw keyboard/gamepad/gesture query returns | Preserve native zero/-1 sentinels and open provider/platform values; use checked enum interpretation explicitly. |
| Trace callback i32 levels, shader locations, buffer/array indices and automation event types | Preserve callback ABI and native extensibility/runtime indices. Type extraction must be explicit at the consumer. |
| Shader/material index enums at raw arrays | A conceptual enum does not turn runtime/native integer index arrays into enum arrays. |
| Mathematical/Unicode/codec/ZIP/PRNG constants | Actual data and standardized bit patterns; not enum categories. |
| Tagged credential/body/result/option/JSON payload types | Cases carry data and are already strongly typed. |
| Small tagged policy/error enums without redundant numeric tables | Their existing names/payload/message branches are already clear; changing representation adds source/display churn without a matching simplification. |
| IO entry_kind | The compiler-owned native metadata bridge materializes this existing tagged shape; changing it would add ABI/layout work with no source mapping benefit. |
| Boolean-to-i64 native adapters | Explicit ABI conversion, not bitmask emulation or a second Boolean type. |
| Per-primitive random arity adapters | Required by the existing numeric facade; generic public sampling bodies already share the actual algorithms. |
| Native resource factories and acquisition leases | Preserve ledger publication, partial failure cleanup, alias authority, cancellation and shutdown. Fallible constructor syntax alone does not prove those ownership contracts. |
| Existing explicit close/drain APIs | Iterator/async sugar could change consumption, integrity or cleanup behavior; no such behavioral rewrite is introduced. |
| Platform-specific providers and public platform checks | Their support contracts remain in native providers. No feature condition would safely replace runtime capability/ownership checks; G87 is exercised in tests/examples rather than used to hide unsupported APIs. |

## Language Bible consolidation

The main Bible is now a concept-organized specification with current contracts,
validated standalone examples and links to feature guides. It covers G78–G87
without chronological implementation-status addenda. The complete accepted G87
text is preserved verbatim after a provenance banner in
[LANGUAGE_BIBLE_HISTORY.md](../LANGUAGE_BIBLE_HISTORY.md), retaining original
relative links, approvals, debates, examples and failure references. Its 24 original
Markdown hard-break/trailing-space lines are deliberately byte-preserved; patch
replay can warn about those historical lines without changing them. Existing
inbound fragment links remain served by explicit compatibility anchors.

Implemented source semantics are separated from a deferred-design table.
G93 `compile func`, optional inline `if condition: return`, organizational Sections,
weak references and other unimplemented directions are not presented as existing
syntax. The canonical toolchain remains LLVM/Clang/LLD 23.1.4. Documentation
navigation now calls the Bible a specification; individual feature guides remain
the detailed contracts. Root README and old milestone evidence are unchanged.

## Qualification and evidence

Evidence root: `/home/shmellyorc/Documents/Void.Language-archives/relocated-documents-2026-11-25/Void.Language-work/g88-modernization`.
Private disk-backed fixture root: `/home/shmellyorc/Documents/Void.Language-archives/relocated-documents-2026-11-25/Void.Language-G88-tmp`.
Exact commands, environment, exit/output, binary hashes and earlier failures are
retained by `qualify.py`, the maintained harnesses and editor launch receipts.
Compiler, server, emitter and native-test children retain 1 MiB soft/hard stacks
and disabled core dumps. Builds and Electron use their ordinary host limits.
Strict runs use the original fatal settings:

```text
ASAN_OPTIONS=detect_leaks=1:halt_on_error=1:abort_on_error=1:exitcode=87
UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1:exitcode=86
```

No suppression or weakened sanitizer setting is added. Native catalog dependencies
are reused from `build/g24-physical/catalog-{debug,release,strict}` with recorded
hashes; they are not installed, republished or silently rebuilt from patched vendors.
The earlier `catalog-sealed-linux` supplied only an obsolete descriptor for this
source surface and was correctly rejected; `raylib-initial.log` retains that input.

### Executed results

The following table records the original pre-correction local Linux x86-64
qualification, not independent acceptance. Focused correction results are recorded
below. Earlier
exploratory failures remain alongside their corrections. Durations include normal
compiler work; these concurrent runs are not performance benchmarks.

| Run | Result and scope |
| --- | --- |
| Debug correction qualification | 6/6 passed (618.73 s): log execution, HTTP source/faults, configuration, console and host resources. |
| Debug final source/ownership/docs slice | 11/13 passed initially (280.61 s); the two refusals were stable-input guards during documentation edits. Frozen-input JSON schema/adversarial rerun passed 2/2 (71.92 s), qualifying all 13 selected gates. Includes current console mapping, random replay, OS identity, binary/IO ownership, generic results, standard-library LSP and the new specification test. |
| Release broader source qualification | 32/33 passed (421.07 s); one console fixture still printed a newly numeric enum directly. Migrated that fixture to `.name`. |
| Release final source/ownership/docs slice | 8/8 passed (42.78 s), including the corrected console fixture, new tests, binary/IO roots, full raylib parity and public docs. This supplies the corrected result for the preceding console failure. |
| Release HTTP application examples | 1/1 passed (52.47 s), against independent local HTTP/TLS peers, including streaming/cancellation and failure classification. |
| Fatal ASan/UBSan/leak qualification | 33/33 passed (1124.81 s), retaining original fatal/leak settings and 1 MiB compiler/native child stacks. |
| Fatal sanitizer final ownership/specification slice | 8/8 passed (113.63 s), including the new inventory/source/LSP tests, binary/IO roots, console and raylib parity/docs after the public comment cleanup. |
| Public comment cleanup and shipped snapshots | Debug and Release each passed 3/3 inventory/parity/docs checks; all three configured package snapshots exactly match the final public source and README. |

`tests/modernization/source.py` passed **24 process checks** in each executed
profile, including JIT/AOT O0/O2, exact numeric contracts, generic nullable enum
values, typed results, dictionary keys, byte codecs, G87 target selection and
all **eight** canonical Bible examples. The invalid constant cast is required to
diagnose a missing enum discriminant. Original new-fixture failures remain separate.

`tests/modernization/raylib.py` passed with the Debug, Release and fatal-sanitizer
compiler/server pairs: **305 discriminants** across **21** numeric/flags types,
JIT/AOT O0/O2, unknown u32 bits, invalid raw/unrelated enum arguments, nullable
specialization, enum dictionary keys, native field/format conversions and native
image cleanup. Its six compiler-owned editor queries check completion, member
source docs, signatures, navigation, references and semantic token kinds.
`raylib-strict-fatal.json/log` records the strict run; the earlier strict exploratory
run without the explicit fatal environment is not substituted for that result.

The maintained resource companion harness passed **22 commands per compiler
profile** for Debug and Release, covering headless and real Linux graphics in
JIT/AOT O0/O2, repeated acquire/release, invalid/stale/extent controls, defaults,
textures, fonts, models and shaders. CPU image ownership/conversions are also
covered under fatal sanitizers. No claim of strict graphics/audio qualification
is made; known unsuppressed provider/driver findings retain their existing records.

The full maintained raylib consumer passed headless JIT/AOT O0/O2, including its
subsystems and retained file/log callbacks, and Star Harbor passed source checking
(**11 commands**, `examples-final.json/log`). Its interactive gameplay/input loop
was not used as a substitute for deterministic library tests. The HTTP examples
above and existing JSON/OS examples cover the changed standard-library callers.

Actual native **Visual Studio Code 1.144.0** and **VSCodium 1.143.1** each passed
five observations: editable-package member docs/definition/references, typed
signature and standard member docs, rendered enum semantic color, the visible
member suggestion popup, and diagnostic/repair with a live server. Both use the
Release server through the 1 MiB wrapper, isolated profiles and the development
extension. No editor/extension installation or version change occurred. The launch
receipts identify editor/server bytes; failed markup assertions are retained too.

All 50 audited library/package inputs are frozen in `frozen-library-inputs.json`.
The final audit checks those bytes, the complete changed-file inventory, binary
hashes, protected native/vendor files and an empty Git index. The complete
historical Bible suffix matches G87 SHA-256
`3e3eeb7ea8af58b93d2a85a6e630c8960b5d0001bb92794d39edbd2861a50454`.

### Reproduction and platform limits

Reuse `build/llvm23-{debug,release,strict}` with LLVM/Clang/LLD 23.1.4.
Build `voidc`, `voidls`, `void_io_runner`, `void_http_source_runner`,
`void_console_source`, `void_binary_ownership` and `void_io_ownership`.
For the HTTP application example gate, also build `void_http_example_host`.
The evidence `qualify.py` records exact bounded CTest selections, stack limits,
private TMPDIR, fatal environment and hashes. Fresh labels preserve earlier runs.

Maintained focused commands (substitute the profile directory):

```sh
ctest --test-dir build/llvm23-release --output-on-failure -R '^(stdlib_modernization_(inventory|source)|lsp_stdlib_modernization|binary_ownership|io_ownership|stage10_console_source)$'
python3 -B tools/raylib/generate.py --check --require-complete --clang .toolchains/llvm-23.1.4/bin/clang
python3 -B tests/modernization/raylib.py build/llvm23-release/voidc build/llvm23-release/voidls build/g24-physical/catalog-release /tmp/g88-raylib-replay.json
```

Retain the fatal settings above for strict children; do not suppress existing
findings. `run-editor.py vscode` and `run-editor.py vscodium` in the evidence root
record actual native client launch commands and the development-extension workflow.
`archive.py` records the accepted-baseline patch replay and archive byte checks.

Native Windows/macOS execution and editor behavior, fresh provider preparation,
user-prefix installation, hosted CI, audio-device exercise and independent Golden
Wrench acceptance remain **UNQUALIFIED in G88**. Existing platform/provider limits
remain in their original reports; this source modernization does not clear them.
No production latency/memory ceiling or performance improvement is claimed.

### Preserved intermediate findings

- Numeric enum display requires explicit `.name`; migrated example text and
  configuration expectations preserve readable names. JSON failure comparisons
  now use direct nominal equality instead of formatted strings.
- Stale pre-G78-linked test runners rejected new enum/bitwise syntax. Relevant
  runners were rebuilt against the accepted current frontend; the original
  `debug-initial.log` is retained, not counted as current qualification.
- Group-writable scratch ancestry was correctly denied by private-directory
  admission. Fixtures now use a newly created task-owned 0700 directory beneath
  safe ancestry; no shared parent permissions or Runtime safety checks changed.
- The first OS exploratory run overlapped compiler relinking and observed a
  changed executable identity. Current qualification runs after builds complete.
- The old JSON LSP fixture expected no signature for a zero-argument generic
  method. The same failure is reproduced with the untouched accepted G87 standard
  sources (`json-lsp-baseline.log`). The fixture now asserts the actual G76 callable
  signature and empty parameter list; no compiler behavior is changed.
- Early new fixtures hit existing no-shadowing, escaped interpolation braces,
  exact unsigned comparisons and the `target_os` predicate spelling. Inputs,
  diagnostics and corrected source receipts are retained. No language restriction
  was relaxed. The Bible examples are all checked and executed by a maintained test.
- Two Debug JSON processes correctly refused qualification when documentation
  edits changed their stdlib input hashes during execution. After freezing all 50
  library/package inputs, both schema and adversarial gates passed unchanged.
  Original receipts and their before/after hashes are retained.
- The first editor assertion compared rendered hover markup with plain spaces;
  the captured hover contained correct source docs with `&nbsp;`. The assertion
  normalizes that presentation markup; native-editor failures/receipts remain.

## Review corrections and focused qualification

The [Golden Wrench review](/home/shmellyorc/Documents/Void.Language-archives/relocated-documents-2026-11-25/void-g88-review-k4wp044i/REVIEW.txt)
identified the keyboard-domain regression, incorrect signed-division prose and
nonexistent automation-event enum reference. These are corrected within G88:

- Seven raw-code operations preserve native key input beyond the closed named
  enum while the existing named calls retain their spelling and nominal types.
- The canonical Bible now states `MIN / -1 = MIN` and `MIN % -1 = 0`, keeping
  integer zero-divisor failures separate. No compiler or numeric behavior changed.
- `automation_event.type` documentation now describes its raw provider `u32`
  number and event-specific payload; its signature remains unchanged.

Correction evidence root:
`/home/shmellyorc/Documents/Void.Language-archives/relocated-documents-2026-11-25/Void.Language-work/g88-review-corrections`.
The original review directory and original archive remain untouched. The original
archive's SHA-256 remains
`93313492d3c18d21cbb7cbf84e22d096c2e06f35c4c380709789231ddd844812`.
Its 57-file inventory identifies the pre-correction candidate, not this correction.
The review includes additional checks in this same implementation chat; it does
not constitute separately authored independent acceptance.

`tests/modernization/raylib.py` now exercises named keys, raw F13=302 and F25=314,
all five query families, exact queue-to-query round trips, exit-key calls, nullable
key-name buffers, and invalid i32 values (minimum, -1, 0, 512 and maximum). Only
valid indices are injected using pinned provider automation event types 1 and 2;
automation payloads are intentionally not used to inject invalid indices.
The injection uses actual native state and queue operations without graphics;
it proves numeric boundary behavior without claiming physical keyboard delivery.
Six negative source cases preserve exact named/raw argument contracts, rejecting
unrelated enums, i64 and float arguments. All 305 discriminants remain checked.
Nine compiler-owned editor queries include raw-code completion, hover and i32
signature help alongside the existing enum queries.

`qualify-keyboard.py` records exact commands, compiler/server/harness hashes,
source bytes, 50 frozen input hashes, native catalogs and the original fatal
ASan/UBSan/leak environment. Compiler, server and native children retain 1 MiB
soft/hard stacks and disabled cores. No provider is rebuilt or installed.
The pre-snapshot runs passed all three profiles; their records remain under
`pre-snapshot/`. Ordinary build refreshes update packaged source and compiler
receipt identities, followed by final focused checks on those exact artifacts.
No unrelated full-suite or numeric semantics rerun was commissioned.

The first exploratory correction run passed all runtime cases but its new hover
assertion expected parameter documentation in a function hover. The captured
hover correctly contains the function description and bounds; parameter details
appear in signature help. The assertion was corrected without changing language
server behavior. Original `raylib-release.json/log` preserves that failure.

Final checks use the refreshed compiler artifacts and unchanged frozen inputs:

| Profile | Result | Launcher wall time |
| --- | --- | --- |
| Debug | 27 records; four JIT/AOT O0/O2 executions and nine editor queries passed | 84.98 s |
| Release | 27 records; four JIT/AOT O0/O2 executions and nine editor queries passed | 25.55 s |
| Strict | 27 records; four JIT/AOT O0/O2 executions and nine editor queries passed | 82.17 s |

All profiles retain the six expected compile-time refusals and 305 discriminant
checks. These are local correctness results, not performance measurements.
Release CTest inventory/public-documentation/native-parity checks passed **3/3**.
Clang 23.1.4 read-only generation verification passed: **798 public functions,
1598 parameters, 545 returns**. Native descriptors, wrappers, provider, header,
layout assertions and primitive declarations remain byte-identical to G87.
`build-snapshots.json` records matching full package copies and compiler/server
hashes for all three refreshed builds.

Actual **VS Code 1.144.0** and **VSCodium 1.143.1** each passed **six observations**,
including raw-code completion, hover and `key: i32` signature help alongside
named-key metadata, visible enum suggestions/color and nominal diagnostic/repair.
`run-editor.py` retains 1 MiB compiler/server limits and records editor/server
identities. `editor-artifacts.json` records the TypeScript compile command and
source/test/client artifact hashes; existing dependencies were reused without
installing anything.

The Bible's eight code examples are byte-identical to the reviewed examples.
The review's independently verified MIN/-1 reproduction and receipts are
preserved; the prose correction does not require a numeric implementation change.
No actual physical-key, initialized key-layout naming, Windows/macOS,
strict graphics/audio, hosted CI or performance acceptance is added.


## Review bundle

Corrected candidate archive:
`/home/shmellyorc/Documents/Void.Language-archives/g88-stdlib-packages-corrected-2026-11-20.zip`,
with adjacent SHA-256 receipt. It contains the exact current candidate, accepted
G87 baseline patch, member hashes, focused correction receipts, editor results and
unchanged original review inputs. The original 57-file archive is embedded as
`prior-review/original-candidate.zip`, preserving its complete successful and
failure evidence without rewriting it. `archive.py` verifies ZIP CRC/member bytes
and exact patch replay against reconstructed accepted-baseline files;
`archive-verified.json` records the corrected digest and result. Rebuildable
binaries, caches, editor profiles and native/toolchain payloads remain outside.

This corrected candidate is returned for focused Golden Wrench re-review. No
independent PASS is claimed. Windows/macOS, strict graphics/audio, hosted CI and
performance acceptance retain their previous qualification limits. Nothing has
been staged, committed, pushed, installed or published.

## Exact changed-file inventory

<!-- G88_CHANGED_FILES -->

58 files against the accepted baseline (including new files):

```text
CMakeLists.txt
docs/DEVELOPMENT_STRATEGY.md
docs/LANGUAGE_BIBLE.md
docs/LANGUAGE_BIBLE_HISTORY.md
docs/README.md
docs/projects/G88_STDLIB_PACKAGES_MODERNIZATION.md
editors/vscode/src/test/modernizationHost.ts
examples/json/configuration.void
examples/json/save.void
examples/net_http/api_json.void
examples/net_http/authenticated_post.void
examples/net_http/cancellation.void
examples/net_http/files.void
examples/net_http/streaming_async.void
examples/raylib-consumer/src/callbacks.void
examples/raylib-consumer/src/main.void
examples/raylib-consumer/src/subsystems.void
examples/star-harbor/app.void
stdlib/README.md
stdlib/console.void
stdlib/encoding/base64.void
stdlib/encoding/binary.void
stdlib/encoding/hex.void
stdlib/io.void
stdlib/json.void
stdlib/log.void
stdlib/net/address.void
stdlib/net/error.void
stdlib/net/http.void
stdlib/net/tcp.void
stdlib/net/udp.void
stdlib/os.void
stdlib/process.void
stdlib/random.void
stdlib/system_error.void
stdlib/url.void
tests/io/async.void
tests/json/configuration.py
tests/json/configuration_fields.void
tests/json/lsp.py
tests/json/typed_smoke.void
tests/modernization/editor.void
tests/modernization/inventory.py
tests/modernization/lsp.py
tests/modernization/raylib.py
tests/modernization/source.py
tests/modernization/stdlib.void
tests/project/raylib_callbacks.py
tests/project/raylib_documentation.py
tests/project/raylib_fallible.py
tests/project/raylib_naming.py
tests/stage10/operator_console_model.void
tools/raylib/enums.py
tools/raylib/generate.py
tools/raylib/inventory.json
tools/raylib/public_docs.py
verified-packages/raylib/README.md
verified-packages/raylib/src/raylib.void
```
