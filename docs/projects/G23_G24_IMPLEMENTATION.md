# G23 packaging and G24 raylib delivery record

Baseline and unchanged HEAD: `ff3bbcd5a20fa2e81506d23e276a8a7837e9404f`.
Qualification date: 2026-11-13. G22 retains its authorized accepted status. G23's
implementation and native editor qualification are preserved. No main-worktree
changes were staged, committed or pushed, and no hosted CI was started.

**G24 implements a usable interface for all 600 declarations: 236 direct C ABI
mappings, 352 ownership adapters, ten G5 lifecycle adapters and two typed
variadic equivalents. The existing 588 API mappings are preserved.** Native
arbitrary C varargs are intentionally unsupported; adapted functions are never
counted as direct C bindings. Boss acceptance and the final public GitHub
post-push consumer qualification remain pending.
Strict graphics and real strict audio also retain independently reproduced
external leak failures. The exclusions and device failures are explicit below;
passing headless or compile checks do not stand in for them.

## Final 600-entry inventory and exposure counts

The original pinned `raylib.h` SHA-256 is
`047e7255f93f8c34039cab906ad76136706b5c7b4c5b5b065d84141963ee9b6b`.
An independent RLAPI scan and Clang 23.1.4 AST must agree on all 600 names.

| Surface | Result |
| --- | --- |
| Public declarations | 600 inventoried; 600 usable APIs with categories below |
| Complete native structs | All 35 size/alignment checks and every field offset |
| Opaque native structs | Two audio types retain their incomplete upstream definition |
| Source type representations | 17 exact POD records; 18 sealed owner/borrow facades; opaque pointers remain opaque |
| Enums | 21 enums, all 305 values preserved as i32 constants |
| Typedefs/macros | 70/46 inventoried; aliases, callbacks, constants and build configuration distinguished |
| Native symbol probe | All 600 original declarations compile, link and resolve on Linux and Windows x64 |

The [inventory](../../verified-packages/raylib/inventory.json) contains each exact
C declaration, binding or blocker, native mapping, ownership and coverage. The
[ownership contract](../../verified-packages/raylib/OWNERSHIP.md) records allocator,
extent, parent, transfer, mutation and terminal policies. Address-bearing source
facades expose readonly field views, not arbitrary raw C pointer assignment.
This does not claim raw mutable C-struct construction in VOID. Native structs
remain exact C backing storage, and their original ABI is checked independently.

| Function exposure | Count | Meaning |
| --- | --- | --- |
| Direct C ABI | 236 | Original scalar/POD signatures through checked forwarding/native-bind |
| Ownership adapter | 352 | Existing resource, span, allocation, result and native-context mappings |
| Callback lifecycle adapter | 10 | Typed context-free routing into retained G5 ownership/barriers |
| Safe typed equivalent | 2 | TraceLog level/text and TextFormat existing VOID formatter |
| Unavailable VOID API | 0 | No placeholder or silent no-op declaration |

The [600-entry inventory](../../verified-packages/raylib/inventory.json) retains
each exact original `c_declaration`, source name, implementation, native ABI,
ownership, exposure category, native limitation and coverage. All former 588
entries retain their original mapping/ownership/implementation metadata (checked
against the preserved baseline package). All 61 baseline files remain present.
`api_accounting_complete` and `complete_usable_api` are true. The legacy
exact-signature `complete_binding` flag remains false with an explicit semantic
label; `complete_direct_c_abi` and `arbitrary_c_varargs_supported` are false.
`--require-complete` checks usable API coverage and now passes.

```sh
python3 verified-packages/raylib/generate.py --clang .toolchains/llvm-23.1.4/bin/clang --check --require-complete
```

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
frees byte buffers and releases its parent. A stream callback/processor retains
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
transfer. PCM is native little-endian x64 bytes (the existing encoding/binary numeric
codec handles float representations); processors are float32 stereo,
independent of the stream's input format. Exhaustion and size/type violations fail
explicitly. Failed generators leave initialized silence; failed processors leave
the input unchanged **and record failure**. G5 faults are observable through the
owner and cleanup, and native failure flags are checked by callback_count/active;
these fallbacks are not reported as successful callbacks. No callback is silently
registered as a no-op or kept alive by an unowned trampoline.

Only representative declarations have runtime exercise. Native symbol presence
and compile-time parity are not reported as runtime execution of all 600 APIs.

## Small reusable ABI changes

The final twelve add no compiler, Runtime or transport ABI change. They reuse
G5 owner-calls/shutdown-owner/consume-always, normal resource factories and byte
spans. CMake only adds a focused G5/raylib reload test executable.

The original conventional bounds increase remains: 1,024 bindings, 128 KiB
descriptor, bounded 256 KiB closure. The verified profile keeps its previous
limits. Existing tests admit the conventional maximum and reject overflow.

Conventional descriptor revision `4 3` extends the existing transport ABI 3:

- Multiple resource operands, resource arguments/results with records or spans,
  and additional factories for one resource type.
- Exact signed C char and fixed arrays of previously declared native records.
- Bounded larger record/field tables for the pinned VR and skeleton layouts.

The backend resolves, roots, deduplicates and pins existing ledger entries in a
stable order. It validates descriptor/type/thunk/release/thread identity and
rejects closed resources. It retains callback failure reporting. Source cannot
manufacture a native address. Existing terminal operations continue through G3;
read-only spans can have non-i64/void results, while writable spans retain the
i64 count contract. No new language syntax, second resource ledger or native
loader was introduced. Internal parent reference counts only retain native
allocation dependencies inside the provider.

The independent mixed C fixture covers two resources (including duplicate aliases),
record arguments/results, owner acquisition from bytes, resource/output spans,
secondary acquisition, C char and inline record arrays. Normal/O2 JIT and
relocated AOT pass, including strict sanitizers and closed-owner rejection.

The package import exception remains narrow: a locked source library may import
only the exact generated native module already admitted by its application.
Frontend and IR checks retain package boundaries, collision refusal, explicit
native approval and `--allow-native`. Existing managed-native tests exercise
missing requirements, project escape and collision failures.

The larger API exposed two editor limits. Completion now filters a typed prefix
before its 256-item cap. Debug discovery measured about 12.4 seconds against the
old ten-second preflight limit (Release about 1.8 seconds). The same guarded
preflight has a 60-second bound and the same 64 KiB response cap. G23's shared task
selection, trust, saved-source guard and literal compiler execution are unchanged.

## Ownership and subsystem implementation

Generation groups ordinary scalar/POD signatures, owned records, counted pointer
results, borrowed views, text, typed input arrays and native out parameters.
Explicit policies handle the relatively few nonuniform transfers and global
contexts. Native-bind still compiles the reviewed descriptors and C prototypes.
There is no alternative package manager, source downloader, registry or installer.

Native buffers carry their allocator, element type, stride and actual count.
Strings are owned terminated UTF-8; static results are copied immediately.
Writable byte operations reject pointer-bearing/typed array aliases, and callers
cannot forge private buffer tags. Independent low-level tests check tag, byte
read/write, text-copy and hash-input rejection before pointer data can escape.

Dependent field/array views retain their native parent and check generations.
Typed input arrays retain their owners and refuse cycles. GPU and audio children
keep their contexts alive. Memory-backed music owns a private copy of its bytes.
Mesh-to-model and texture-to-material operations transfer ownership explicitly;
source owners are consumed only on successful transfer. Active automation lists
unregister before destruction. Failed record acquisition releases partial native
allocations, including the material allocation in a failed model load.

Model cleanup releases public currentPose/boneMatrices allocations and deduplicates
its material shader/texture identities before the original native terminal.
Animated image data includes all returned frames. Multi-frame images are readonly;
copy a single image before pointer-based image mutation. Shader uniform extents
cover the full 6.0 float/int/uint-vector/sampler enum, without the older eight-type
limit. Native operation, format, dimension and valid-index preconditions still
apply; private mesh-buffer limits are not guessed.

The maintained consumer exercises:

| Mode | Representative real native operations |
| --- | --- |
| Headless | Nested camera/matrix/ray/collision ABI; image copy/draw/resize; PNG memory encode/decode; two-frame GIF extent and parent retention; hashes/compression/Base64; strings/codepoints/text arrays; file results; WAV decode/copy/crop/sample cleanup; failed image/wave loads |
| Visible graphics | Window/input/drawing; texture mipmaps and material transfer; mesh/model rendering and views; failed model cleanup; font/glyph borrows; shader/framebuffer calls; actual uint-vector shader pixel readback on Linux |
| Approved file assets | Native font loading, allocated glyphs/atlas rectangles; IQM skeletal model/animation/pose views; animation updates; automation-list recording lifecycle |
| Real audio | Device initialization; sound alias retention/playback states; memory music after input close; stream updates; terminal order and device shutdown |

The optional assets come from the already approved archive with SHA-256
`2b3ee1e2120c7a0796b33062c7e9a694dd8a8caa56a96319ac8c8ecf54a90d0b`.
The extraction helper downloads nothing and preserves model/font notices and the
font's embedded license. The Git fixture places them in ordinary D5 resources;
relocated graphics loads the packaged copies. No physical VR headset, every
renderer feature or runtime execution of every declaration is claimed.

## Actual source package and D3/D4/D5 workflow

The five-profile revisions in the following baseline table are preserved evidence
for the original 588 interfaces. The final-interface replays are recorded separately
below; no public GitHub fetch is inferred from either disposable fixture.


The source library remains `verified-packages/raylib`, ID `org.voidlang.raylib`,
version `0.1.0`. Its native requirement is independently `6.0.0`, ABI `3`, approved
catalog revision `raylib-6.0-api-v3`. No native binaries are checked in. Pinned
upstream bytes are preserved by `.gitattributes`.

The application declares HTTPS Git origin, exact full lowercase SHA, package
subdirectory and exact source version. The existing smart HTTPS fixture creates
a real disposable commit and runs public `deps fetch`, resolve, read-only locked
resolve/check/explain, explicit native authorization, normal/O2 JIT and native
AOT. It creates real D5 packages, verifies them, relocates to space/Unicode paths,
verifies again and executes from another writable working directory. It tests
changed source, revision, lock and native-provider refusal, plus actual LSP hover
and prefixed completion. Private buffer rejection is part of the same replay.
Native preparation and operator approval remain separate from Git acquisition.

| Compiler profile | Disposable immutable revision | Result |
| --- | --- | --- |
| Linux debug | `bd00b282178fb82751ea9886d384577736d23fb1` | PASS: implemented-surface Git/JIT/AOT/D5, guards and LSP |
| Linux release | `352e8097f428ad523091cc242771c48ebd693251` | PASS: implemented-surface Git/JIT/AOT/D5, guards and LSP |
| Linux strict | `0b8a89cfe1f638303d2167261497f6704ea8c0a8` | PASS: implemented-surface Git/JIT/AOT/D5, guards and LSP |
| Windows Debug | `c62cbd5901f7105d153d1a651961a61a4e31e8a5` | PASS: implemented-surface Git/JIT/AOT/D5, guards and LSP |
| Windows Release | `63890150d7388eda2fcddab557153e7f9e075b84` | PASS: implemented-surface Git/JIT/AOT/D5, guards and LSP |

Those revisions belong to disposable fixture repositories, not public GitHub.
The final public fetch has not been run: the integration commit does not yet
exist. The fixture's success does not supply or invent that final SHA.

## Preserved native profiles and editor results

All builds use pinned LLVM/Clang/LLD 23.1.4. Linux profiles are Debug, Release and
strict ASan/UBSan with fatal errors and leak detection. Windows uses the existing
Windows 10 Home build 19045 guest (VM name `Windows 11`), x64 VS 2022 Native Tools
17.14.42, VCTools 14.44.35207 and Windows SDK 10.0.19041.0. Native shell evidence
records both host and target architecture as x64. Guest compiler-source overlays
were restored after the builds; no guest integration commit was created.

| Profile | Focused regressions | Editor regressions | Real pinned-package workflow |
| --- | --- | --- | --- |
| PikaOS Debug | 13/13 | 6/6 LSP; native VS Code 1.141.0 passes four consumer checks | Git/JIT/AOT/D5 passes |
| PikaOS Release | 13/13 | 6/6 LSP; native VS Code 1.141.0 passes four consumer checks | Git/JIT/AOT/D5 passes |
| Linux strict | 13/13 | 6/6 LSP | Full headless Git/JIT/AOT/D5 passes; device leak failures below |
| Windows 10 Debug | 11/11 applicable tests | Native VS Code 1.141.0 passes four consumer checks | Git/JIT/AOT/D5, real audio and visible JIT/relocated D5 pass |
| Windows 10 Release | 11/11 applicable tests | Native VS Code 1.141.0 passes four consumer checks | Git/JIT/AOT/D5, real audio and visible JIT/relocated D5 pass |

The Windows Debug result combines its ten passing applicable regressions with the
corrected independent mixed-ABI test replay. Release passed all eleven together.
Subsequent editor-only C++ filtering changes were exercised by the real native
package/LSP/editor workflows. The Linux focused set covers D3/D4/D5 packages,
managed-native resolution/admission, G1/G2/G3/G4/G5 and managed callbacks. The
additional six LSP regressions cover recovery, protocol and fields/display.

Linux's final Debug JIT and relocated Release D5 graphics runs both pass the
unchanged pixel, input, module-origin and clean-exit assertions, including the
new shader and allocation-failure checks. Real Linux audio passes functional
playback and cleanup; its strict leak outcome is recorded separately below.

The existing G11 headless package fixture passes separately on all three Linux
profiles: normal/O2 JIT/AOT, isolated relocation, scoped trace and artifact
refusal. Existing Windows G11 receipts remain preserved; the new software-renderer
consumer adds fresh device/JIT/D5 evidence. G23 retains its earlier 37 native editor
checks per Linux/Windows Debug/Release profile. Editor unit tests pass 8/8. The
four new consumer checks are typed native-record hover, late-alphabet prefixed
completion, clean source diagnostics and the actual Check task.

The Windows upstream GLFW DLL cannot create a window with the guest's WGL 1.1
capability. Fresh graphics qualification uses the exact approved source's Win32
software renderer, built with the pinned native toolchain. It is not reported as
Linux OpenGL feature parity. The native 600-symbol probe and complete ABI asserts
pass against this library. Linux uses the approved upstream shared release.

Preserved failed attempts include canonical descriptor CRLF in the new Windows
fixture (fixed by writing bytes), a negative-test message oracle, protected default
working directory (fixed by using the existing private scratch), test packet
prerequisites, preflight timeout and completion-before-filter truncation. These
were repaired without lowering pass criteria. UI tests wait for the real drawing
loop's ready marker before input/pixel checks; pixel, input and exit requirements
remain intact. No unrelated full suite or hosted CI is claimed.

## Final-interface qualification

| Profile | Final callback / reload / failure suite | Fresh focused checks | Final Git/JIT/AOT/D5 and editor |
| --- | --- | --- | --- |
| Linux Debug | PASS, normal/O2 and real audio, including mixer stress | 13/13 | PASS; five native editor checks |
| Linux Release | PASS, normal/O2 and real audio, including mixer stress | 13/13 | PASS; five native editor checks |
| Linux strict | PASS, normal/O2 and real audio, including mixer stress | 13/13 | PASS; strict callback ownership clean; full-device leaks retained |
| Windows Debug | PASS, normal/O2 and real audio, including mixer stress | 9/9 | PASS; five native editor checks; visible JIT/relocated D5 |
| Windows Release | PASS, normal/O2 and real audio, including mixer stress | 9/9 | PASS; five native editor checks; visible JIT/relocated D5 |

| Final consumer | Disposable immutable Git revision |
| --- | --- |
| Linux debug | `d6b2e30819c5f0cd731f23173de71d82a2c2e875` |
| Linux release | `f2ef5cf3f317c8ecbf788cca7e9969106ceedab9` |
| Linux strict | `1ca3ffd7ca039e094a45177470be663647ca049b` |
| Windows Debug | `f68a6a8fa4ca9abd3ab0c94c86f422fb810edaab` |
| Windows Release | `cf82754dee56f8ace24f3ced6dad2f496012c4d3` |

These are local smart HTTPS fixture commits. The public post-push GitHub run is
still required. Both normal and optimized program profiles run under each
compiler build. The original 61-file qualification is preserved above. Eight
editor unit checks pass with the matching server. The legacy scoped trace and
managed-native/D5 package regression passes again on all three Linux builds.
Linux Debug JIT and relocated Release D5 graphics pass fresh pixel/input/origin
checks; full Debug/Release audio passes after bounded polling for actual callback
invocations, including reopening the audio device. This keeps the invocation
requirements and gives device startup up to five seconds.

Final fixes found during qualification: Windows export declarations now agree;
global replacement keeps the admission gate closed; audio routes remain owned
and published until the native mixer barrier returns; G5-sealed audio entry is
shutdown cancellation while actual G5 faults remain reported by the owner.
The editor hover oracle decodes Markdown underscore escapes. Earlier failed
attempts and the timing oracle are retained in the evidence directories.

The focused callback suite runs normal/O2 JIT and relocated native AOT. It tests
all five file/log global hooks, literal percent/UTF-8 logging, int/float/bool/text
formatting and invalid format results, invocation and replacement of every global
hook, exact native load/unload handoff and ordinary load/save rejection. It tests
managed callback faults, self-close rejection, global/audio coexistence rejection,
double-close, callback slot exhaustion, mismatched detach, invalid PCM lengths
and forbidden recursive raylib calls from audio callbacks.

Real audio tests invoke the stream generator plus stream and mixed processors,
replace the generator, detach processors and close source parent owners before
the final callback. Implicit shutdown leaves a live playing callback for G5/Runtime
to drain. A thousand-cycle generator replacement and mixed-processor attachment/
detachment stress test runs in both AOT profiles. Normal/O2 native JIT reload tests
force GC while a closure is retained, replace the program, invoke old captured
code, unregister, verify old generation retirement and acquire replacement code.
No new callback framework or Runtime policy was needed.

The final consumer imports these interfaces from the actual Git source package;
its headless path runs typed format/log and all five retained global hooks in JIT,
AOT and D5 relocation. Real device checks remain separate. Native editor tests
cover the retained callback signature as well as existing hover/completion,
diagnostics and the actual Check task. G23's preserved Package Project behavior,
D3/D4/D5 and verified-package layout remain unchanged.

## Strict leak classification

No sanitizer suppression or relaxed exit criterion is used:

```text
ASAN_OPTIONS=halt_on_error=1:abort_on_error=1:detect_leaks=1
UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1
```

| Native control / path | Result | Classification |
| --- | --- | --- |
| No-window native graphics control | Exit 0 | No observed leak |
| Native C window, three frames, CloseWindow | Abort, 256 bytes | PCs inside Gallium executable mapping |
| Expanded strict VOID graphics | Cleanup/pixel assertions reached, then abort, same 256 bytes | External graphics dependency; unfixed |
| Native C audio device only | Exit 0 | No observed leak |
| Native C WAV music load/update/unload | Abort, 408 bytes in LoadMusicStreamFromMemory | Pinned raylib omits freeing the WAV context after drwav_uninit |
| Native C sound alias load/unload | Abort, 64 bytes in LoadAudioBuffer | Pinned UnloadSoundAlias omits converterResidual release |
| Expanded strict real audio | Playback/cleanup assertions reached, then abort, 408 + 64 bytes | Same upstream raylib allocations; unfixed |
| Final callback-only strict headless and real audio | Exit 0, including replacement/detach, implicit shutdown, failure cleanup and mixer stress | VOID, G5 and adapter-owned resources clean; does not exercise leaking music/alias operations |

The graphics control contains no VOID, shim or graphics harness. It records the
library mappings before CloseWindow. PCs at Gallium offsets `0x9188aa` and
`0x918f92` are within its executable mapping. Native C audio controls retain the
raylib mapping, so ASan resolves the two allocation functions directly. Native
source confirms both omitted frees. The binding uses the matching original
terminal operations; opaque audio internals are not guessed or freed through a
private ABI. These failures remain failures, and are not claimed fixed by this
work. Strict headless, mixed-resource and buffer-rejection paths pass.

The final independent controls were rerun with the pinned library and sanitizers:
no-window and device-only exit 0; window, music and alias controls abort with the
same 256/408/64 bytes. Fresh graphics PCs match the same Gallium offsets. The
combined final strict audio consumer reaches both original subsystem and callback
cleanup markers, then reports exactly 472 bytes in two allocations. Its retired
native closure is already unloaded at LSan symbolization, so those frames display
as unknown modules; the independently linked native controls identify the native
allocation functions. No missing symbols are presented as new attribution proof.
The final callback-only strict suite passes independently, including real audio,
implicit cleanup and the thousand-cycle mixer stress, with no sanitizer reports.

## Evidence and Git handoff

Final-interface receipts, logs, stress/reload tests and native catalogs are
retained under `build/g24-last12/`, with final machine evidence in its `handoff/`.
Windows final-interface evidence is under
`/home/shmellyorc/VM/g24-final-20261113/`.

Preserved baseline receipts, XML, logs, pixel captures, packages and native controls
are retained under `build/g24-continuation/`. The preserved baseline machine receipt is
`build/g24-continuation/handoff/receipt.json`. Native Windows receipts and scripts
are under `/home/shmellyorc/VM/g24-completion-20261113/`; the guest artifacts remain
under `C:/Users/a/VoidG24CompleteNative`. Earlier G23/G24 evidence under
`build/g24-evidence/` and `/home/shmellyorc/VM/g23-g24-20261112/` remains preserved.
Expected device failures and earlier diagnostic attempts are retained separately
from passing receipts. These local artifacts are not a public release bundle.

The exact changed-file inventory below includes the original uncommitted G23/G24
work and this continuation. The main index is empty. `git diff --check` passes;
new files are checked separately with `git diff --no-index --check`. The pinned
upstream license/header retain their original bytes and declared attributes.

`git status --short --untracked-files=all` records 30 modified tracked files and
39 untracked files: 69 paths total, with an empty index. The full status snapshot,
per-file SHA-256 values and machine results are in
`build/g24-last12/handoff/git-status.txt` and
`build/g24-last12/handoff/receipt.json`. Final native input bytes match all three
qualified Linux descriptors and the qualified Windows source packet. Generated
source API bytes also match the Windows packet. HEAD remains the baseline SHA.

69 changed files; 0 staged:

```text
.gitattributes
CMakeLists.txt
docs/EDITOR_INTEGRATION.md
docs/projects/G23_G24_IMPLEMENTATION.md
docs/roadmap/MILESTONE_REGISTER.md
editors/vscode/README.md
editors/vscode/package.json
editors/vscode/src/config.ts
editors/vscode/src/extension.ts
editors/vscode/src/test/projectHost.ts
editors/vscode/src/test/raylibHost.ts
editors/vscode/src/test/runRaylibHost.ts
examples/raylib-consumer/README.md
examples/raylib-consumer/configure.py
examples/raylib-consumer/src/callbacks.void
examples/raylib-consumer/src/main.void
examples/raylib-consumer/src/subsystems.void
examples/raylib/native/raylib_provider.c
examples/raylib/native/raylib_provider.h
examples/raylib/prepare.py
include/void/c_dynamic.hpp
include/void/c_primitive.hpp
src/backend/llvm/backend.cpp
src/dynamic_closure.cpp
src/frontend/compiler.cpp
src/frontend/project.cpp
src/frontend/project_native.cpp
src/frontend/project_selection.inc
src/ir/c_primitive.cpp
src/ir/module.cpp
src/main.cpp
src/runtime/c_dynamic.cpp
src/runtime/c_resource.cpp
src/runtime/c_span.cpp
tests/ffi/mixed.descriptor.in
tests/ffi/mixed.void
tests/ffi/mixed_fixture.c
tests/ffi/mixed_fixture.h
tests/ffi/primitives.cpp
tests/ffi/raylib_callback_reload.cpp
tests/project/git_https.py
tests/project/managed_native.py
tests/project/mixed_native.py
tests/project/raylib_assets.py
tests/project/raylib_audio_control.c
tests/project/raylib_callbacks.py
tests/project/raylib_graphics.py
tests/project/raylib_graphics_windows.py
tests/project/raylib_guards.py
tests/project/raylib_leak_control.c
tests/project/raylib_native_symbols.py
tests/project/verified_raylib.py
verified-packages/README.md
verified-packages/raylib/OWNERSHIP.md
verified-packages/raylib/README.md
verified-packages/raylib/callback_bindings.py
verified-packages/raylib/generate.py
verified-packages/raylib/inventory.json
verified-packages/raylib/native/raylib_api.c
verified-packages/raylib/native/raylib_api.descriptor.inc
verified-packages/raylib/native/raylib_api.h
verified-packages/raylib/native/raylib_callbacks.inc
verified-packages/raylib/native/raylib_parity.h
verified-packages/raylib/owned_bindings.py
verified-packages/raylib/src/api.void
verified-packages/raylib/src/callbacks.void.inc
verified-packages/raylib/upstream/LICENSE
verified-packages/raylib/upstream/raylib.h
verified-packages/raylib/void.project.toml
```

After Boss's review and eventual integration/push:

1. Use the actual integrated full lowercase SHA in a fresh external consumer via
   `configure.py --rev SHA`; do not reuse a disposable fixture revision.
2. Run public GitHub `deps fetch` from the declared origin/subdirectory and verify
   the source/native lock, read-only locked checks and explicit native catalog.
3. Repeat normal/O2 JIT, AOT and real D5 verify/relocation on the required hosts,
   with native editor and device checks appropriate to each renderer.
4. Retain the typed-varargs distinction, global-hook exclusivity and bounded PCM
   callback contracts, plus unsuppressed native device leak failures. Do not turn
   an equivalent API into a direct-binding claim or call these device paths green.

No staging, integration commit, push, hosted CI or public GitHub fetch was performed
on Boss's behalf.
