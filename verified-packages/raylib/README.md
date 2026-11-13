# raylib 6.0 — editable VOID source package

All 600 declarations have usable interfaces: 236 direct C ABI mappings,
352 ownership adapters, ten G5 callback lifecycle adapters and two typed
variadic equivalents. Arbitrary C varargs remain unsupported. Exact declarations,
exposure categories and native limits are in [inventory.json](inventory.json) and
[OWNERSHIP.md](OWNERSHIP.md).

## Source and native layout

```text
void.project.toml                 library/package identity and entry
src/raylib.void                   public implementation; edit this normally
src/raylib_native.void            explicit, descriptor-checked native declarations
native/raylib.descriptor.in       native descriptor template
native/raylib_api.descriptor.inc  complete native signature/layout declarations
native/raylib_provider.c/.h       native context and gallery implementation
native/raylib_api.c/.h            resource/value adapters
native/raylib_callbacks.inc       C routing into existing G5 ownership
native/raylib_parity.h            exact upstream C prototype/layout assertions
upstream/raylib.h                 pinned raylib 6.0 header
upstream/LICENSE                 upstream license
prepare.py                       explicit native preparation from approved local inputs
```

The package entry is `src/raylib.void`. It imports `./raylib_native.void`; both
files are real source. The declaration file starts with
`native "org.voidlang.raylib"`. The approved descriptor uses
`source-file /src/raylib_native.void`. The generic C interop system verifies the
provider, complete declaration inventory, sealed resources, records, function
signatures and declaration-only pass bodies. It then uses the existing checked C
transport. The compiler and Runtime contain no raylib-specific mappings.

There is no virtual binding source or VOID `.inc` dependency. Normal check/build
reads these `.void` bytes; it never regenerates them. Edit a wrapper or add a
function in `raylib.void`, refresh the local dependency lock, and build normally.
Edits to fetched Git snapshots remain tampering and are rejected. Use a local path
dependency for development, then publish a new immutable Git commit.

`generate.py --check --clang /pinned/clang --require-complete` maintains native
C parity and declaration accounting; it checks public API names without imposing
generated implementation bodies. It does not overwrite either VOID source file.
The optional explicit `voidc native-bind DESCRIPTOR raylib_api.h -o NEW.void`
maintenance operation exports native declarations to a **new** file only. Ordinary
source edits require neither generator nor export command.

## Dependencies and native approval

Source ID/version: `org.voidlang.raylib`, `0.1.0`. Native ID/version:
`org.voidlang.raylib`, `6.0.0`, ABI `3`, catalog revision
`raylib-6.0-api-v4-physical`. The compiler must support generic physical C
declarations (`source-file`/`native`); the older baseline compiler is insufficient.
LLVM/Clang/LLD remain pinned to 23.1.4.

Canonical development stays at `verified-packages/raylib/` in VOID.Language.
The proposed standalone repository contains **this directory's contents at its
root**, including this manifest and the package-owned C implementation. Publication
requires Boss authorization and an integration commit containing the physical
conversion. No standalone repository has been published by this task.

A normal standalone consumer declares:

```toml
[dependencies.raylib]
id = "org.voidlang.raylib"
git = "https://github.com/Shmellyorc/VOID.raylib.git"
rev = "<actual full lowercase standalone package commit SHA>"
version = "=0.1.0"

[native-requirements.raylib]
id = "org.voidlang.raylib"
version = "=6.0.0"
abi = 3
```

The URL is proposed and uncreated; the SHA placeholder is documentation, not a
valid pin. Boss chooses the repository and actual immutable commit. No `subdir`
is needed because the standalone library manifest is at the repository root.
The existing example supports `configure.py --origin URL --rev SHA --standalone`.
The full monorepo exceeds D3's existing limits; those limits are unchanged.

Git acquisition downloads/verifies source only. Approve and prepare native code
separately using the existing command and trusted local raylib library:

```sh
python3 prepare.py --voidc /path/to/voidc --clang /pinned/clang \
  --header upstream/raylib.h --library /approved/libraylib.so.6.0.0 \
  --license upstream/LICENSE --managed-catalog /new/approved/catalog
export VOID_NATIVE_CATALOG=/new/approved/catalog
voidc deps fetch --project /consumer
voidc resolve --project /consumer
voidc resolve --locked --project /consumer
voidc deps explain --project /consumer --json
voidc check --project /consumer
voidc run --project /consumer --allow-native --profile debug
voidc run --project /consumer --allow-native --profile release
voidc package --project /consumer --allow-native -o /new/package
voidc package --verify /new/package
```

On Windows use the x64 VS 2022 Native Tools shell, `python`, `raylib.dll` and its
matching `--import-library /approved/raylib.lib`. On strict Linux add `--sanitize`
to preparation. Preparation consumes package-owned native source; it does not
rewrite VOID. Packages carry approved runtime/link/notices through existing D4/D5.

## Using the API

```void
use pkg/raylib as rl

func main():
    var image = rl.gen_image_color(16, 16, rl.orange()) as rl.image
    defer rl.unload_image(image)
    rl.image_draw_pixel(image, 2, 3, rl.blue())
    print(rl.get_image_color(image, 2, 3).b)
```

Names are snake case. Native pointers stay opaque; owner terminals consume their
handles and borrowed fields retain/check parents. Native operation, thread,
format, dimension and index preconditions still apply.

`trace_log(level, text)` uses native `TraceLog(level,"%s",text)`.
`text_format(value, spec = "")` reuses VOID's typed formatter and returns a result;
use interpolation for multiple values. Supported values retain the existing
numeric/bool/string and display/format-hook contracts. These are typed equivalents,
not arbitrary C varargs bindings.

Callback setters/attachments return an owned `callback_registration`. Close or
matching detach unregisters and drains before G5 retires managed state/code. Global
hooks require an exclusive application-thread window without audio. PCM callbacks
cannot recursively call raylib; processors use float32 stereo bytes. Sixteen audio
slots and bounded I/O are explicit limits. See [OWNERSHIP.md](OWNERSHIP.md).

Strict full-device leaks remain unsuppressed: 256 bytes in Gallium, 408 bytes in
raylib's WAV music context and 64 bytes in the sound-alias converter residual.
Clean strict headless/bounded callback results do not make these device paths clean.
