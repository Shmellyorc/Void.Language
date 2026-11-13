# raylib 6.0 source package

**600/600 usable APIs: 236 direct C ABI mappings, 352 ownership adapters, ten
callback lifecycle adapters and two typed variadic equivalents.** Original C
varargs are not exposed.
Package ID `org.voidlang.raylib`, source version `0.1.0`, native provider version
`6.0.0`, transport ABI `3`. The complete declaration list and individual mapping
or technical blocker are in [inventory.json](inventory.json).

The bindings cover window/input, shapes/collision, images/textures/render targets,
fonts/text, meshes/materials/models/animation, shaders/VR, files/compression/hashes,
automation and wave/sound/music/streams. [OWNERSHIP.md](OWNERSHIP.md) specifies the
native types, field views, allocations, transfers, terminal operations, callback
lifetimes and observed upstream leaks. All original C declarations retain their
native compile assertions. The 35 complete structs, two opaque audio types,
21 enums, 305 enumerators, 70 typedefs and 46 macros are inventoried separately.
A native probe compiles, links and resolves all 600 function symbols.

## Source dependency and native approval

Copy `examples/raylib-consumer` to a writable directory. After Boss integrates
and pushes the implementation, configure the actual full lowercase commit SHA:

```sh
python3 configure.py --rev "$RAYLIB_COMMIT"
voidc deps fetch --project .
```

`configure.py` refuses a floating revision or an existing manifest. It writes:

```toml
[dependencies.raylib]
id = "org.voidlang.raylib"
git = "https://github.com/Shmellyorc/Void.Language.git"
rev = "<actual full lowercase 40-character integration SHA>"
subdir = "verified-packages/raylib"
version = "=0.1.0"

[native-requirements.raylib]
id = "org.voidlang.raylib"
version = "=6.0.0"
abi = 3
```

The displayed placeholder is documentation, not a usable pin. The final public
GitHub fetch requires the eventual integration commit. Qualification uses a
disposable real Git commit over the existing D3 smart HTTPS fixture.

Native approval remains a separate explicit operation, using trusted local
raylib files and pinned Clang/LLVM/LLD 23.1.4:

```sh
python3 examples/raylib/prepare.py --voidc /path/to/voidc \
  --clang /path/to/clang --header verified-packages/raylib/upstream/raylib.h \
  --library /path/to/libraylib.so.6.0.0 --license /path/to/raylib/LICENSE \
  --api-package verified-packages/raylib --managed-catalog /new/approved/catalog
export VOID_NATIVE_CATALOG=/new/approved/catalog
```

On Windows run in an x64 VS 2022 Native Tools shell, use `python`, `raylib.dll`
and its matching `--import-library /path/to/raylib.lib`. The renderer must be
supported by the machine. The Windows 10 guest requires the pinned source's
Win32 software renderer; its shader/framebuffer capabilities differ from Linux
OpenGL. For strict Linux add `--sanitize`. Preparation consumes reviewed generated
files and uses existing native-bind, catalog and dynamic-closure infrastructure.
Git dependency acquisition neither builds nor authorizes native code.

```sh
voidc resolve --project .
voidc resolve --locked --project .
voidc deps explain --project . --json
voidc check --project .
voidc run --project . --allow-native --profile debug
voidc run --project . --allow-native --profile release
voidc build --project . --allow-native -o /new/aot/app
voidc package --project . --allow-native -o /new/package
voidc package --verify /new/package
```

Move the whole package and verify/run it from another working directory. D5
verifies the executable, native payload, licenses and declared resources. VS Code
1.141.0 uses the same locked package and approved catalog for typed hover,
completion and Check. Editor tasks retain their existing explicit native execution
boundary; they do not silently add `--allow-native`.

## Using values and owners

```void
use pkg/raylib as rl

func main():
    var pixels = rl.gen_image_color(16, 16, rl.orange()) as rl.image
    defer rl.unload_image(pixels)
    rl.image_draw_pixel(pixels, 2, 3, rl.blue())
    print(rl.get_image_color(pixels, 2, 3).b)
    var codepoints = rl.load_codepoints("Aé🎮") as rl.int_array
    defer rl.unload_codepoints(codepoints)
    print(rl.int_array_get(codepoints, 2))
```

Names are generated snake case. Use explicit ownership-consuming unloads or the
existing Runtime cleanup. Borrowed fields retain their native parent and reject
stale generations. Native pointers never become source integers. Close functions
return i32 ledger status; out pointers become counted buffers, result fields or
owner metadata. Source strings cross as owned terminated UTF-8. Nullable C text
uses `c_string?`, while C text results use nullable `byte_buffer` and `buffer_text`.
Native C preconditions and application-thread rules still apply.

The consumer defaults to headless ABI and ownership checks. `--window` exercises
public resource APIs and an interactive gallery. `--audio` exercises a real audio
device, aliases, memory-backed music, playback states and stream updates. Optional
font/animation inputs can be extracted from the already approved source archive:

```sh
python3 tests/project/raylib_assets.py /approved/raylib-6.0-source.tar.gz /new/assets
export VOID_TEST_RAYLIB_ASSETS=/new/assets
```

The Git consumer fixture copies those inputs into ordinary D5 application
resources. Relocated graphics qualification loads them from the package resource
directory. They are test inputs, not another dependency downloader.

## Final typed interfaces and native limits

`trace_log(level, text)` uses native TraceLog with a literal "%s" format.
`text_format(value, spec = "")` delegates to VOID's existing typed formatter and
returns a result; use interpolation for multiple values. Numeric types, bool,
string and existing nominal display/format hooks retain their ordinary supported
format syntax. Neither interface implements arbitrary C varargs.

All ten callback declarations have retained G5 adapters. Setters/attachments
return an owned `callback_registration`; close or the matching detach unregisters
and drains it before releasing the managed closure. File handlers exchange owned
bytes/text, with native MemAlloc/Unload handoff. Audio generation uses the stream
format; processors use float32 stereo bytes. Global hooks require an exclusive
application-thread window without audio. Audio callbacks cannot recursively call
raylib. Sixteen audio slots and bounded I/O are explicit limits. See the exact
signatures, replacement, failure and shutdown rules in [OWNERSHIP.md](OWNERSHIP.md).

Supported ABI targets are Linux x86-64 GNU/SysV and Windows x64 MSVC. Native
symbol presence and headless checks do not establish every device/backend feature
or runtime exercise of every declaration. Strict graphics and real strict audio
remain failed for independently reproduced external allocations: 256 bytes in
Gallium, 408 bytes in raylib's WAV music context and 64 bytes in its sound-alias
converter residual. Leak detection remains enabled; none is suppressed or claimed
fixed. See the [consolidated qualification record](../../docs/projects/G23_G24_IMPLEMENTATION.md).

## Reproduce generated parity and package checks

```sh
python3 verified-packages/raylib/generate.py --clang /pinned/clang --check --require-complete
python3 tests/project/raylib_native_symbols.py /pinned/clang verified-packages/raylib /approved/catalog
VOID_TEST_RAYLIB_CATALOG=/approved/catalog \
VOID_TEST_RAYLIB_EVIDENCE=/new/evidence \
python3 -B tests/project/git_https.py /path/to/voidc /path/to/git
```

`generate.py --check --require-complete` verifies all 600 usable interfaces.
`complete_usable_api` is true; the legacy exact-signature `complete_binding` flag
and `complete_direct_c_abi` remain false. Typed equivalents are counted separately. The pinned header SHA-256 is
`047e7255f93f8c34039cab906ad76136706b5c7b4c5b5b065d84141963ee9b6b`.
Generation inspects the existing Clang AST/layouts, verifies an independent RLAPI
name inventory and emits ordinary native-bind inputs. It introduces no compiler
syntax, registry, replacement package manager or second native loader.
