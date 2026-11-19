# G77 — Fallible raylib resource loaders

Baseline `713a4492de5583608189f2f5bc374e35708a0cd3` (G69 on G76).
Native Linux qualification, 19 November 2026. No staging, commit, push,
publication or user-prefix installation is authorized or performed.

## Implementation

Added **34 source-level fallible companions**, using the existing lowercase
`result<t, load_error>` with `Ok`, `Err` and `?`. They call the existing public
raylib wrappers and use the existing validity queries and consuming terminals.
All **757 original functions are byte-identical in the original source prefix**.
The 600-function native inventory, physical native declarations, generated C,
header/parity and ABI descriptor bytes are unchanged. No compiler, Runtime, LLVM,
FFI authorization or native ownership implementation changed.

### Public API inventory

| Companion | Ok value | Successful owner cleanup |
| --- | --- | --- |
| `try_load_image` | `image` | `unload_image` |
| `try_load_image_raw` | `image` | `unload_image` |
| `try_load_image_anim` | `image` | `unload_image` |
| `try_load_image_anim_from_memory` | `image` | `unload_image` |
| `try_load_image_from_memory` | `image` | `unload_image` |
| `try_load_image_from_texture` | `image` | `unload_image` |
| `try_load_image_from_screen` | `image` | `unload_image` |
| `try_load_texture` | `texture` | `unload_texture` |
| `try_load_texture_from_image` | `texture` | `unload_texture` |
| `try_load_texture_cubemap` | `texture` | `unload_texture` |
| `try_load_render_texture` | `render_texture` | `unload_render_texture` |
| `try_load_font` | `font` | `unload_font` |
| `try_load_font_ex` | `font` | `unload_font` |
| `try_load_font_from_memory` | `font` | `unload_font` |
| `try_load_shader` | `shader` | `unload_shader` |
| `try_load_shader_from_memory` | `shader` | `unload_shader` |
| `try_load_wave` | `wave` | `unload_wave` |
| `try_load_wave_from_memory` | `wave` | `unload_wave` |
| `try_load_sound` | `sound` | `unload_sound` |
| `try_load_sound_from_wave` | `sound` | `unload_sound` |
| `try_load_sound_alias` | `sound` | `unload_sound_alias` |
| `try_load_music_stream` | `music` | `unload_music_stream` |
| `try_load_music_stream_from_memory` | `music` | `unload_music_stream` |
| `try_load_audio_stream` | `audio_stream` | `unload_audio_stream` |
| `try_load_model` | `model` | `unload_model` |
| `try_load_file_data` | `byte_buffer` | `buffer_close` |
| `try_load_file_text` | `byte_buffer` | `buffer_close` |
| `try_export_image_to_memory` | `byte_buffer` | `buffer_close` |
| `try_compress_data` | `byte_buffer` | `buffer_close` |
| `try_decompress_data` | `byte_buffer` | `buffer_close` |
| `try_save_file_data` | `bool` | No new owner; inputs remain caller-owned |
| `try_save_file_text` | `bool` | No new owner; inputs remain caller-owned |
| `try_export_image` | `bool` | No new owner; inputs remain caller-owned |
| `try_export_wave` | `bool` | No new owner; inputs remain caller-owned |

The other new public declarations are documented `load_error_kind` (four members)
and managed `load_error extends core_error.error`, with `operation: string` and
`kind: load_error_kind`. Inherited `message`, `path` and `code` retain their normal
standard-error conventions. The producing helper fills `message`, `operation`,
`kind` and the API-specific code; `path` retains its default empty string because
not every operation has a path or an identified filesystem cause.

## Error and ownership contracts

- `acquisition_failed` (code 1): the existing acquisition returned no owner.
- `invalid_resource` (code 2): a candidate failed the normal validity query and
  was released before returning the error.
- `default_fallback` (code 3): a requested font or custom shader resolved to the
  identifiable native default instead. Its temporary wrapper is released; the
  raylib-owned default itself is not destroyed.
- `operation_failed` (code 4): the existing save/export operation returned false.
  Successful status companions return `Ok(true)`.

No missing-file/permissions/decoder-specific cause is invented. `operation` names
the underlying public operation. Successful results retain the existing one-owner
contract: aliases/copies do not make independent native allocations or permit
repeated releases. Byte buffers use their recorded producer's terminal through
`buffer_close`. Sound aliases retain their source and use `unload_sound_alias`.
Music loaded from memory retains the existing native private byte copy.

Font companions check font metadata, its texture validity and default-font
texture identity. Temporary font/texture views are explicitly released. Shader
companions check native validity and compare against the native default program
when custom input was requested. Both null shader arguments intentionally request
the default and may succeed. A nondefault program does not establish that every
requested stage was used; the underlying API exposes no complete per-stage
status. A valid model can retain raylib's normal default materials.

Native guard failures (stale/closed owners, invalid extents, missing context,
wrong thread) and Runtime allocation failures retain their checked failure
behavior; these APIs do not catch or reclassify them into recoverable errors.
Tests compare original/companion native rejection messages and exit 1.

The initial inventory deliberately covers common file/memory loading and useful
file/data results. Transfer APIs such as `load_model_from_mesh`, unrelated
allocation/view helpers and the rest of the 600 C API remain available through
their existing interfaces; no blanket Result rewrite was attempted.

## Source documentation and developer experience

Current coverage is **791 public functions, 1,591 ordered parameters and 539
value-returning functions**, retaining the original 757/1,524/505 baseline. Every
new function has a description, every declared parameter has its ordered
`## @param`, and each return documents `Ok`/`Err`, owner cleanup and limitations.
The new error enum, its four members, error type and fields have attached `##`
docs too. The two working example functions are also documented.

The new documentation verifier checks all 34 exact signatures against their
existing companions, lowercase result types, parameter docs, error/ownership
return docs and error declarations. The original twenty documentation-meaning
mutations remain; four additional broken companion cases must be rejected.
`raylib_public_documentation` registers the source-only check with CTest.

Actual pinned-Clang regeneration and `--check --require-complete` both pass.
Regeneration leaves the public source/docs and all native generated bytes
unchanged. The public `.void` module is physical source, not an output rewritten
by native preparation. Coverage does not count these helpers as new C ABI calls.

`examples/raylib-consumer/src/fallible.void` demonstrates explicit result matching,
`?` propagation and `defer` cleanup. `transcode_image` releases its loaded image
whether export succeeds or fails; `describe_image` logs only the known failure.
The package README has a complete example, inventory, context/ownership rules and
default-resource caveats. The ownership and consumer guides are updated.

## Bounded upstream research

Pinned raylib 6.0 is tag commit `dbc56a87da87d973a9c5baa4e7438a9d20121d28`.
Existing adapters and that source were authoritative:

- [Textures/images](https://github.com/raysan5/raylib/blob/6.0/src/rtextures.c):
  resource validity is stronger than a non-null allocation/nonzero FBO ID. A
  zero-sized framebuffer can acquire an ID but fail `IsRenderTextureValid`.
- [Fonts](https://github.com/raysan5/raylib/blob/6.0/src/rtext.c): several load
  paths return `GetFontDefault`; `IsFontValid` alone does not check the GPU
  texture or distinguish the requested asset from that fallback.
- [Shaders](https://github.com/raysan5/raylib/blob/6.0/src/rcore.c): null stages
  select defaults, and failed/missing custom input can resolve to a default
  program. Default shaders are raylib-owned during unload.
- [Models](https://github.com/raysan5/raylib/blob/6.0/src/rmodels.c): material
  fallback is normal and does not necessarily make an otherwise valid model a
  failed acquisition.
- [Audio](https://github.com/raysan5/raylib/blob/6.0/src/raudio.c): decoded waves,
  sound aliases, streams and music retain distinct release/lifetime rules.
- [raylib-rs texture/image Results](https://github.com/deltaphc/raylib-rs/blob/master/raylib/src/core/texture.rs)
  informed the Result-shaped companions. No implementation/framework was copied;
  speculative rich decoder errors were not adopted.

These are existing implementations, not proposed fixes to import. VOID's sealed
owners and the pinned native interface remain the compatibility boundary.

## Verification

All compiler profiles use LLVM/Clang/LLD **23.1.4**. The new native test builds an
isolated physical package and real PNG/GIF/raw-image/WAV/OBJ/GLSL fixtures. The
native graphics font input is the host's DejaVuSans.ttf; no download occurs.
Source ranges are inclusive: image round trips run 33 iterations (`0..32`), and
GPU/audio cases run nine (`0..8`). Nothing touches existing projects or approvals.

| Check | Result |
| --- | --- |
| Expanded Release compiler native matrix | **26/26 commands PASS** |
| Expanded Debug compiler same matrix | **26/26 commands PASS** |
| Fatal ASan/UBSan/leaks expanded headless matrix | **18/18 commands PASS** |
| Existing + extended raylib documentation/native/LSP regression | PASS with Release, Debug and strict compilers |
| Registered pure documentation test | **1/1 PASS per profile** |
| Actual VS Code 1.144.0 | **9/9 checks PASS** |
| Actual VSCodium 1.143.17232 test copy | **9/9 checks PASS** |
| Rebuilt extension tests | **13/13 PASS**, no skips |
| Actual regeneration / native inventory check | PASS; public source retained, native output hashes unchanged |
| Final voidc/voidls builds | PASS in Release, Debug and strict |
| Final uninstalled extension VSIX | Packaged successfully; existing icon assets preserved |

The native matrices cover JIT and AOT at both O0 and O2, real headless/GPU/audio
success and failures, all 34 companions, unsupported data, missing assets,
zero-sized returned resources, rejected font/shader fallbacks, intentional default
shaders, invalid extents and stale ownership. They check repeated cleanup, source
sound retention and memory-backed music after closing its input bytes. Original
native guard classifications remain failures. Existing non-fallible operations
remain exercised alongside their companions. No compiler redesign or unrelated
full-suite run was required.

The three documentation regressions additionally compile/run named calls through
JIT/AOT O0/O2 and check old and new LSP hover, completion, signature return types,
ordered parameter descriptions, active named parameters and valid-edit recovery.
Real editor runs use isolated development hosts/profiles and the matched rebuilt
tools; no extension is installed into the user's profile. They check readable
result types, owning-result docs, the public error's docs, named active parameter
labels and the actual rendered parameter widget. Screenshots were captured;
VS Code's final image was visually inspected. Markdown's escaped spaces and
punctuation are normalized in provider assertions rather than treated as prose
or rendering defects.

### Strict native-device failures remain failures

No sanitizer suppression, disabled leak detection, weakened native check or
larger-stack workaround was used. Fatal settings were:

```text
ASAN_OPTIONS=detect_leaks=1:halt_on_error=1:abort_on_error=1:exitcode=87
UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1:exitcode=86
```

The expanded strict graphics path completes the G77 assertions and cleanup, then
aborts with the existing **256-byte / one-allocation** window/graphics-driver
leak. The repository's independent plain-C `raylib_leak_control.c`, containing no
VOID or adapter, reproduces the same total after `CloseWindow`; its no-window
control exits 0. This is not recorded as a strict graphics pass.

Strict real audio similarly completes its assertions/cleanup, then aborts with
**7,920 bytes / 27 allocations**. Existing plain-C alias/music controls reproduce
64-byte `LoadAudioBuffer` and 408-byte `LoadMusicStreamFromMemory` leaks, while
init/close alone passes. A matching nine-iteration plain-C workload reproduces
**exactly 7,920 bytes / 27 allocations**: nine alias residuals plus eighteen music
residuals. The first C comparison used eight iterations (7,040/24); that result is
preserved and was corrected for VOID's inclusive `0..8` range. These native
release limitations were already documented in the ownership record. They are
not hidden, reclassified as passing or broadened into a new milestone HOLD.
No additional G77-specific allocation family is exposed by the comparison.

Native Windows/macOS remain **UNQUALIFIED**. Linux graphics/audio functional
success does not imply strict leak cleanliness or another native platform pass.
The upstream library is the pinned release binary; the VOID shim, Runtime and
compiler were instrumented for strict runs. No broad malformed-native-file parser
fuzzing or universal renderer/decoder guarantees are claimed.

### Preserved unsuccessful intermediate evidence

- First/second new fixture drafts used unsupported `is not`/bare result-pattern
  grammar; corrected to existing negated payload patterns, without compiler changes.
- A negative test first expected the wrong checked failure string. Final checks
  compare original and companion failure messages, not a weakened exit expectation.
- A documentation mutation first hit a private helper signature; it now mutates
  the complete public companion signature.
- The first editor assertion compared raw escaped Markdown as plain text; the
  final harness normalizes that representation and verifies the real UI too.
- An initial intended audio invocation omitted `--audio`; its recorded mode is
  headless and is never counted as audio acceptance.
- All strict device failures and the eight-iteration C comparison remain in the
  raw evidence alongside the final controls and successful headless results.

## Performance

Existing 757 function bodies and native implementations are unchanged. New APIs
add validity checks and temporary default-identity queries where required; no
optimization tuning was performed. An isolated before/after namespace benchmark
used the same release server and fixture; the candidate differs in the raylib
source/README/ownership files and an explicitly refreshed local lock. Each case
uses 100 samples, five warmups, instrumentation disabled and no competing task
builds/tests. Values are protocol round trips, not renderer timing:

| Case | Original package median / p95 / worst ms | G77 package median / p95 / worst ms |
| --- | ---: | ---: |
| Existing call | 20.84 / 24.75 / 34.18 | 20.68 / 21.89 / 23.62 |
| Newline | 20.83 / 21.73 / 26.90 | 21.18 / 29.36 / 33.93 |
| EOF | 20.69 / 21.48 / 26.26 | 20.95 / 21.91 / 25.93 |
| Immediate edited EOF | 18.05 / 41.64 / 42.26 | 17.50 / 41.97 / 43.41 |

Warm medians remain about 21 ms. The single-pair newline p95 rises from 21.73 to
29.36 ms and is retained; other cases differ less or improve. This is a small
regression check, not proof of a universal latency bound or a new G77 target.
Cold project-scaling and native I/O throughput were not comprehensively profiled.

## Rebuilt test artifacts and installed-tool distinction

The user's requested final rebuild completed for `voidc`, `voidls` and the
extension. Ready-to-test release executables:

```text
/home/shmellyorc/Documents/Void.Language/build/llvm23-release/voidc
/home/shmellyorc/Documents/Void.Language/build/llvm23-release/voidls
/tmp/void-g77-hxris3dj/artifacts/void-language-g77-0.1.2.vsix
```

Debug and strict executables are under `build/llvm23-debug` and
`build/llvm23-strict`. Each rebuilt compiler's shipped raylib source snapshot is
byte-identical to the final G77 physical source. VSIX contents include the G69
icons and current G76 extension implementation; G77 needs no TypeScript product
change. The test VSIX has not been installed or published.

The installed `~/.local/bin/voidc` and `voidls` still resolve to
`~/.local/opt/void-g72-7ef5ba6d/bin/`; the installed extension is
`~/.vscode/extensions/void-language.void-language-0.1.2`. These locations were
identified read-only and remain unchanged. To test the rebuilt binaries, use
the explicit paths above and configure a disposable workspace's
`void.server.path` / `void.compiler.path` accordingly. Replacing installed tools
or installing the VSIX remains subject to the directive's separate approval.
Existing locked projects also retain their old package source until it is
explicitly updated; their locks are not silently rewritten to obtain G77.

## Evidence and changed-file inventory

Structured API/test/editor data, native C controls, raw performance samples,
artifact/source hashes and limitations are in
[evidence/g77/results.json](evidence/g77/results.json). Selected successful and
unsuccessful logs are in [evidence/g77/verification.txt](evidence/g77/verification.txt).
All raw fixtures, native controls, screenshots, failing intermediates and the
VSIX remain under `/tmp/void-g77-hxris3dj`.

Current exact candidate inventory:

```text
CMakeLists.txt
docs/README.md
docs/projects/G77_FALLIBLE_RAYLIB_LOADERS.md
docs/projects/RAYLIB_PUBLIC_API_DOCUMENTATION.md
docs/projects/evidence/g77/results.json
docs/projects/evidence/g77/verification.txt
examples/raylib-consumer/README.md
examples/raylib-consumer/src/fallible.void
tests/project/raylib_documentation.py
tests/project/raylib_fallible.py
tests/project/raylib_fallible_editor.cjs
tools/raylib/README.md
tools/raylib/public_docs.py
verified-packages/raylib/OWNERSHIP.md
verified-packages/raylib/README.md
verified-packages/raylib/src/raylib.void
```

`git diff --check` and new-text-file whitespace checks pass. No stage, commit,
push, hosted CI, publication, user-prefix installation, user settings modification
or worktree/recovery cleanup occurred. The work is ready for focused review with
the strict native-device limitations explicitly retained.
