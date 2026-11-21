# Add a package and use raylib

A source package brings reusable VOID declarations into your project. A native
package also needs a verified provider and separate permission to execute it.
[Projects and modules](https://github.com/Shmellyorc/Void.Language/wiki/Projects-and-modules) explains `use pkg/...` imports.

## Add the bundled package

After [installing the tools](https://github.com/Shmellyorc/Void.Language/wiki/Installation), you can add the bundled library to a fresh application.

The bundled raylib preparer is written in VOID. First add uses pinned HTTPS
release bytes, SHA-256 checks and the selected Clang/LLD; it needs no Python or
CMake. Linux additionally uses native tar/gzip for the official tar.gz asset.
Windows uses the VOID ZIP reader. Later imports and builds reuse the locked
provider. Host development and device libraries remain platform requirements.

```sh
voidc new app --name PixelDemo
cd PixelDemo
voidc package list
voidc add raylib
voidc tidy --check
```

The matching compiler supplies the package definition and preparation inputs.
Adding raylib can prepare missing native artifacts and require downloads and
host prerequisites. It writes project source under `libs/raylib` and updates
`void.project.toml` and `void.lock`. Source addition does not grant native consent.

## A first native operation

Replace `src/app.void` with this complete program:

```void
use pkg/raylib as rl

func main() -> i32:
    var image = rl.gen_image_color(16, 16, rl.orange()) as rl.image
    defer rl.unload_image(image)
    rl.image_draw_pixel(image, 2, 3, rl.blue())
    var pixel = rl.get_image_color(image, 2, 3)
    if pixel.b != 241:
        return 1
    print(pixel.b)
    return 0
```

It prints `241`, the blue channel of that pixel. It uses a CPU image, so no window
or audio device is needed. The resource is explicitly unloaded. Production code
can handle a loader's nullable or typed fallible result instead of assuming
acquisition succeeds; see [Errors and nullable values](https://github.com/Shmellyorc/Void.Language/wiki/Errors-and-nullable-values).

```sh
voidc check .
voidc run .
voidc build . --reuse-native-approval
```

Run in an ordinary interactive terminal. Inspect the displayed native provider and
type `approve` only if you trust it. Editor tasks can then reuse matching approval.
Changed provider/lock inputs need a new review. Automation can deliberately pass
`--allow-native`; that option is explicit consent, not a fix for a broken lock.

## Typed input and raw key codes

Raylib's 305 provider enum/flag values are preserved in 21 named VOID types.
Use `rl.keyboard_key.escape` for a named key and combine masks such as
`rl.config_flags.window_hidden | rl.config_flags.window_resizable`.
Native record fields still use their exact primitive representations: compare
camera `projection` with `rl.camera_projection.perspective.value`.

Here is a separate complete headless query example:

```void
use pkg/raylib as rl

func main():
    rl.set_exit_key(rl.keyboard_key.escape)
    var provider_key: i32 = 302
    print(rl.is_key_down_code(provider_key))
    var queued = rl.get_key_pressed()
    if queued != 0:
        print(rl.is_key_down_code(queued))
```

Without injected or live input it prints `false`. It demonstrates passing a raw
code through the API, not hardware delivery. GLFW F13 is 302 and F25 is 314, but
neither is a named case in the pinned raylib header. A failed `as? keyboard_key`
conversion does not make such a provider code invalid.

Each named keyboard API has an `i32` companion: `is_key_pressed_code`,
`is_key_pressed_repeat_code`, `is_key_down_code`, `is_key_released_code`,
`is_key_up_code`, `set_exit_key_code` and `get_key_name_code`. Queue results can
be passed directly to these operations. Pinned state queries return false for
codes <= 0 or >= 512, including the up query. Zero disables the exit key.
Names may be unavailable and require an initialized keyboard backend; successful
name buffers must be closed with `buffer_close`.

## Ownership and portability

Config and gesture flags preserve unknown `u32` bits; other numeric enums check
named membership. Extract `.value` at raw native calls and record boundaries.
A copied native wrapper shares resource authority. Follow unload/close and
callback unregister-and-drain requirements in the
[package ownership guide](https://github.com/Shmellyorc/Void.Language/blob/main/verified-packages/raylib/OWNERSHIP.md).

Native locks are target-specific; use artifacts matching your OS and architecture.
CPU images need no window or audio device. Graphics, audio and keyboard behavior
depends on the native backend and device availability. The
[package guide](https://github.com/Shmellyorc/Void.Language/blob/main/verified-packages/raylib/README.md) contains platform prerequisites,
resource APIs and examples. [Native interoperability](https://github.com/Shmellyorc/Void.Language/wiki/Native-compilation#use-a-native-library)
explains the boundary before writing your own provider.

Next: [Editor setup](https://github.com/Shmellyorc/Void.Language/wiki/Editor-setup).
