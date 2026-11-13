# Native ABI and ownership

This package accounts for the pinned raylib 6.0 header's 600 public declarations.
All 600 have usable interfaces: 236 direct C ABI mappings, 352 ownership adapters,
ten lifecycle adapters and two typed equivalents. `complete_usable_api` is true.
The legacy `complete_binding` exact-signature flag remains false because arbitrary
C varargs are not implemented; `complete_direct_c_abi` is also false. Counts never
classify typed equivalents or ownership/lifecycle adapters as direct bindings.

The native compiler checks all 600 original prototypes, all 35 complete struct
layouts and every field offset. Two audio structs remain opaque exactly as in the
header. It checks all callback typedefs and 305 enumerator values. The separate
native symbol probe links and resolves all 600 names; it does not call them all.

## Physical source declarations

`src/raylib.void` is the editable public implementation. Its ordinary relative
import selects the physical `src/raylib_native.void` declaration module. The
`native` provider marker and descriptor `source-file` path bind that module through
the existing generic C ABI verifier; signature/record/resource mismatches and
non-declaration bodies fail checking. No virtual binding source is created.
Local package edits require an explicit lock refresh; Git snapshot edits remain
integrity failures. Native preparation does not run a VOID generator.

## Reused transport

Conventional descriptor revision `4 3` extends the existing ABI 3 word transport.
It admits multiple typed resource operands, resource/record/span combinations,
additional factories for one resource type, exact C `char`, and inline arrays of
records. The Runtime resolves and pins the existing sealed ledger entries in a
stable order, including duplicate arguments. Native pointers never become source
integers. Old descriptor revisions retain their prior validation limits.

POD records remain exact native values: Vector2/3/4, Matrix, Color, Rectangle,
NPatchInfo, Camera2D/3D, Transform, BoneInfo, Ray, RayCollision, BoundingBox,
VrDeviceInfo, VrStereoConfig and AutomationEvent. Aliases resolve to their native
canonical type. Fixed arrays retain their extent, stride and nested record shape.
Enums and flags are exact i32 constants. C build-configuration macros stay C
configuration. `GetFileModTime` widens the native C long to a portable source i64;
the native call still uses the platform's actual long prototype.

Address-bearing records use sealed owners or parent-retained views backed by the
real C struct. Their source classes are not substitutes for the C memory layout.
Every C field retains its checked native offset. Field access is readonly;
`Image.format` and `Texture.format` are named `pixel_format` in VOID because
`format` is a reserved display hook. Internal pointers with no public extent
(shader locations, mesh VBO IDs, audio internals and decoder context) stay opaque.
Use raylib's public query functions for those internals.

## Terminal operations

| Owner | Native terminal operation | Dependencies |
| --- | --- | --- |
| window | CloseWindow | Retained title; GPU descendants drain first |
| audio_device | CloseAudioDevice | Sound, music and stream descendants drain first |
| image | UnloadImage | Atlas rectangles also belong to the atlas image |
| texture | UnloadTexture | Window |
| render_texture | UnloadRenderTexture | Window; attached textures are borrowed |
| font | UnloadFont | Window; glyphs, rectangles and texture are borrowed |
| mesh | UnloadMesh | Window for generated/uploaded GPU meshes |
| shader | UnloadShader | Window; upstream owns its default shader |
| material | UnloadMaterial | Window; transferred textures belong to the material |
| model | UnloadModel plus public allocation cleanup | Window; mesh/material/skeleton views retain the model |
| wave | UnloadWave | CPU allocation |
| audio_stream | UnloadAudioStream | Audio device |
| sound | UnloadSound or UnloadSoundAlias | Alias records its terminal variant and retains its source |
| music | UnloadMusicStream | Audio device; memory loader retains a private native copy |
| file_path_list | UnloadDirectoryFiles | Paths are borrowed from the list |
| automation_event_list | UnloadAutomationEventList | Active recording unregisters before terminal release |

Failed record acquisitions release partial native allocations before returning
null. Native model release deliberately leaves GPU material objects to the C caller and
omits public `currentPose`/`boneMatrices` allocations in this pinned source. The
adapter owns those allocations, releases them, and deduplicates shared material
shader/texture identities before the upstream model release. It uses public map
slots, without inventing a private map count.

GlyphInfo, MaterialMap, ModelSkeleton and ModelAnimation views retain their
parent allocation. Closing a source parent owner prevents its reuse but defers
native destruction until dependent views close. Views carry generation checks:
after a mutation, an old view is rejected before native memory access. Reallocating
image, wave, mesh and texture operations require an exclusive owning object. Animated image data exposes all returned frames. Multi-frame images are readonly;
copy a single image before applying pointer-based image mutations. Borrowed
arrays are readonly. Typed input arrays retain their element owners and validate
their generations; setters reject ownership cycles.

`load_model_from_mesh` transfers the mesh allocation and consumes its source
owner on successful acquisition. `set_material_texture` transfers an exclusive
texture into an exclusive material, releases the old map texture and consumes the
source texture owner. A borrowed material cannot receive an ownership transfer.
The selected shapes texture is retained until replacement or window close.

All owners use the existing owner-thread and shutdown-owner rules. Explicit
terminal operations return the ledger status (i32); successful native void
unloads are therefore represented by consuming close operations. Aliases of a
closed source owner cannot close or use the allocation a second time. Runtime
shutdown releases outstanding ledger owners through the same native terminals.

## Allocated and static results

| Producer family | Source representation | Original release |
| --- | --- | --- |
| MemAlloc/MemRealloc | byte_buffer | MemFree |
| LoadRandomSequence | int_array | UnloadRandomSequence |
| LoadFileData | byte_buffer | UnloadFileData |
| LoadFileText | byte_buffer containing C text | UnloadFileText |
| Compress/Decompress/Base64 and image export memory | byte_buffer | MemFree |
| LoadImageColors/Palette | color_array | Matching image-array unload |
| LoadFontData | glyph_info_array | UnloadFontData with actual count |
| LoadMaterials | material_array | Material GPU cleanup, then MemFree |
| LoadModelAnimations | model_animation_array | UnloadModelAnimations with actual count |
| LoadUTF8/LoadCodepoints | byte_buffer/int_array | Matching UTF-8/codepoint unload |
| LoadWaveSamples | float_array | UnloadWaveSamples |
| LoadTextLines | string_array | UnloadTextLines with actual count |
| Text*Alloc | byte_buffer | MemFree |
| Static text, TextSplit and hash words | Immediate owned copy | Copy uses MemFree |

Buffers record allocator, type, stride and actual count. Private byte operations
and raw byte-input functions reject aliases of pointer-bearing or typed arrays;
callers cannot forge a buffer tag to read or overwrite native pointer fields. Closing a buffer always
uses its recorded producer's terminal; it does not infer an allocator from the
source variable's spelling. Count-taking unloads check the actual count. Text
input is copied to terminated UTF-8 and rejects embedded NUL. Optional C strings
use `c_string?`, with null remaining a native NULL; shader defaults and empty
automation lists therefore remain expressible. Static C text results are copied
before another native call can overwrite them. `buffer_text` decodes through the
first bounded NUL; `buffer_bytes` preserves all counted bytes.

String-array results are copied deeply where native storage is static. Dropped
paths are deep snapshots: the original event allocation is acknowledged with
UnloadDroppedFiles during acquisition, so later event polling cannot invalidate
the source result. Closing the snapshot releases only the copied allocation.

Pointer inputs validate their native extent and element type. Byte transfer uses
the existing bounded 64 KiB span bridge in chunks. VOID arrays are not pinned for
indefinite native retention. Music-from-memory owns a separate native copy, so
closing or changing the source byte buffer cannot invalidate streaming playback.
Realloc preserves the original owner on allocation failure and refuses a live
borrow or a non-byte allocation.

Native output pointers become counted buffers, named result fields, or owner
metadata: collision point/hit, codepoint value/size, image animation frame count,
font glyph count and allocated result length are preserved. Camera pointer updates
return the updated exact value. TextAppend returns the updated byte cursor.

## Typed variadics and retained callbacks

The original declarations remain in the 600-entry inventory and native parity
header, including `void TraceLog(int logLevel, const char *text, ...)` and
`const char *TextFormat(const char *text, ...)`. Neither arbitrary C varargs nor a
source `va_list` is transported. `trace_log(level, message)` accepts a log enum
value and owned UTF-8 text; its minimal C adapter calls
`TraceLog(level, "%s", text)`, so percent sequences are text. Native filtering and
fatal-level behavior are retained. `text_format<t>(value, spec = "")` returns
`result<string, string>` using the existing VOID `format(value, spec)` builtin.
Supported built-in values are numeric types, bool and string; nominal values and
collections use the existing display/format hooks and their ordinary type checks.
The spec is VOID's invariant format syntax. Use ordinary interpolation for
multiple typed values. Invalid specs return Err. There is no duplicate formatter,
printf parser or claim of C TextFormat's rotating static buffer/varargs semantics.

All ten context-free declarations use one small native routing adapter to G5's
existing context-first `i32(u32, i32)` retained callback. G5 owns the closure,
managed captures, generation lease, entry barrier and retirement. Native owned
registrations route only to that context; no managed address escapes as an
integer. Invocation data crosses bounded, invocation-scoped byte spans copied
into owned VOID values. No second callback ownership framework is introduced.

| Original declaration | Native lifecycle and VOID implementation |
| --- | --- |
| SetTraceLogCallback | Retained global setter; calls are synchronous within native TraceLog. `set_trace_log_callback(func(i32, string))` formats va_list natively before invoking VOID. |
| SetLoadFileDataCallback | Retained global setter; synchronous file-load invocation. Handler `func(string) -> result<[u8], string>`; successful bytes are copied with MemAlloc for the original UnloadFileData owner. Err means native load failure. |
| SetSaveFileDataCallback | Retained global setter; synchronous file-save invocation. Handler `func(string, [u8]) -> bool`; input is copied before managed use. False is ordinary save rejection. |
| SetLoadFileTextCallback | Retained global setter; synchronous text-load invocation. Handler `func(string) -> result<string, string>`; NUL-free UTF-8 is copied with MemAlloc for UnloadFileText. |
| SetSaveFileTextCallback | Retained global setter; synchronous text-save invocation. Handler `func(string, string) -> bool`; both native inputs are copied. |
| SetAudioStreamCallback | Retained stream-owned generator, invoked synchronously by the mixer thread. Handler `func(frames:u32, channels:u32, sample_size:u32) -> [u8]` must return exactly the requested PCM bytes in the stream format. Replaces and drains the prior generator. |
| AttachAudioStreamProcessor | Retained stream-owned ordered processor. `func([u8], frames:u32) -> [u8]` receives float32 stereo bytes and must return the same length. Each attachment gets its own native callback identity. |
| DetachAudioStreamProcessor | Unregister/drain operation; `detach_audio_stream_processor(registration)` consumes the matching processor registration and rejects other kinds. |
| AttachAudioMixedProcessor | Retained audio-device-owned ordered processor with the same float32 stereo handler shape. |
| DetachAudioMixedProcessor | Unregister/drain operation; consumes the matching mixed registration and rejects other kinds. |

Registration returns `callback_registration`. `callback_count` and
`callback_active` inspect it; `callback_close` unregisters and consumes it. Global
setter replacement makes the former registration inactive while its G5 closure
remains owned until close/Runtime shutdown. Closing an inactive former owner
cannot unregister its replacement. Closing the active owner restores native NULL
behavior, rather than a previous callback. Explicit close and implicit Runtime
cleanup both unregister before managed context/generation release. Closing or borrowing a
callback registration from its own invocation is rejected by G5. Reentrant
invocation of the same registration is rejected. The historical scoped gallery
trace helper and the retained trace hook mutually exclude each other.

The five native global setters are unsynchronized plain pointer stores: there is
no native cross-thread drain primitive. Their supported contract is an exclusive
application-thread window **without an audio device**. Installing these hooks
while audio is live, initializing audio while a global hook is live, changing a
hook inside a callback, or foreign-thread calls through the generated provider
while globals are registered are rejected. Global installation/replacement and
provider call admission share a gate; registration requires no provider calls in
flight. Native callers outside this provider must honor the same exclusive
window. Concurrent arbitrary third-party C use is a precisely unsupported native
case, not a fictitious G5 guarantee.

Audio unregistration uses raylib 6.0's own mixer mutex: SetAudioStreamCallback(NULL)
and both Detach operations wait for the current mixer invocation. The adapter
performs that native barrier while its route remains owned/published, then
unpublishes it, drains entered routes and
frees its byte buffers and releases its parent. A stream callback/processor retains
its stream and the stream's device; a mixed processor retains the device. A
source stream/device owner may close first without freeing that native parent
prematurely. Shutdown drains callbacks before the underlying device can close.
PCM handlers cannot call raylib/provider operations recursively: raylib invokes
them while holding its audio mutex. The provider rejects such calls, records
failure and G5 reports the managed error. Use owned invocation data and ordinary
VOID computation; shared captures still require ordinary thread synchronization.
This managed copying path does not promise hard real-time latency.

Bounds: sixteen simultaneous audio registrations per loaded provider, 64 MiB of
copied data per invocation, 64 KiB formatted trace text and 64 KiB per span
transfer. PCM is native little-endian x64 bytes (use the existing encoding/binary numeric
codec for float representations); processors are float32 stereo,
independent of the stream's input format. Exhaustion and size/type violations fail
explicitly. Failed generators leave initialized silence; failed processors leave
the input unchanged **and record failure**. G5 faults are observable through the
owner and cleanup, and native failure flags are checked by callback_count/active;
these fallbacks are not reported as successful callbacks. No callback is silently
registered as a no-op or kept alive by an unowned trampoline.

## Native preconditions and observed upstream leaks

Call drawing, GPU and window operations with a live window, on its application
thread. Call device operations while its audio context or descendants remain
live. Native format, dimension, index and operation-order preconditions still
apply. These wrappers do not turn every invalid C argument into a recoverable
error. In particular, raylib GetTime/WaitTime uses the initialized window timer;
the headless audio test uses VOID's existing monotonic clock.

Fatal ASan/UBSan and leak detection remain enabled in strict qualification. The
expanded graphics path reaches all cleanup assertions, then fails with the same
256-byte Gallium allocation reproduced by the independent native C window
control. Real strict audio reaches cleanup, then fails with 408 bytes from native
LoadMusicStreamFromMemory (WAV context) and 64 bytes from native LoadSoundAlias
(converter residual). Separate C controls reproduce each native leak; device-only
initialization/shutdown passes. The pinned raudio.c omits the corresponding frees.
These upstream/host allocations remain unfixed and unsuppressed. They are not
reported as passing strict graphics or strict real audio.

The final callback-only strict suite separately passes headless and real audio
invocation, replacement, implicit shutdown and failure cleanup without sanitizer
reports. It avoids the independently leaking music/alias paths and does not
reclassify those full-device failures as green.
