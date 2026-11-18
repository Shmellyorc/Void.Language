# raylib for VOID

This library wraps raylib 6.0 for graphics, input, audio and image processing.
It exposes all 600 supported interfaces in snake case. The API includes 236
direct C ABI mappings, 352 ownership and value adapters, 10 callback
interfaces and two typed alternatives to C variadic functions.

Public wrappers document their parameters and return values in source. Parameter
names follow the pinned C declaration in snake_case. VOID rejects shadowing,
so names already used by a public type or constructor receive `_value`.
For example, the circle signature is
`draw_circle(center_x: i32, center_y: i32, radius: f32, color_value: native.color)`.
The `color()` constructor keeps its established name.

Calls using the former anonymous labels must adopt the descriptive names.
For example, replace `get_random_value(p0 = 1, p1 = 6)` with
`get_random_value(min = 1, max = 6)`. Positional argument ordering and types
are preserved. Adapted APIs document VOID-owned buffers, nullable results,
callback lifetimes and the matching cleanup operation.

## Add raylib to an application

The bundled library supports x86-64 GNU/Linux and Windows x64 with the MSVC
ABI. Use matching VOID tools and the configured Clang/LLD 23.1.4 toolchain.
On Windows, use an x64 Visual Studio Native Tools environment so Clang can
find the Windows SDK and C libraries.

The first add needs Python 3.12 or newer on PATH and HTTPS access to the
pinned official raylib 6.0 release. The compiler uses its installed
preparation tool to verify the release and build the checked native provider.
Subsequent imports, checks, runs and builds use the locked source and
prepared provider. They do not invoke Python or regenerate the bindings.

```sh
voidc new app --name MyGame
cd MyGame
voidc package list
voidc add raylib
```

In VS Code, open the application folder in a trusted window, save your
changes and choose **VOID: Add Library** → **raylib** from the Command
Palette. Both routes install the package under `libs/raylib` and update
`void.project.toml` and `void.lock`.

Replace `src/app.void` with this example. It draws one pixel into a memory
image and needs neither a display nor an audio device.

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

```sh
voidc tidy --check
voidc check .
voidc run .
voidc build . --reuse-native-approval
```

Run `voidc run .` in an ordinary interactive terminal. Review the displayed
native provider and type `approve` if you trust it. The example prints
`241`. **VOID: Run Project**, **VOID: Build Project** and F5 can reuse the
matching approval after that review. A different lock or native provider
requires a new review. List or revoke approvals with `voidc native approvals`
and `voidc native revoke APPROVAL_ID`.

## Read or edit the package

| Path | Purpose |
| --- | --- |
| `void.project.toml` | Declares a library project and a package with its own ID and entry |
| `src/raylib.void` | Public API and editable VOID wrappers |
| `src/raylib_native.void` | Physical declarations for the checked native provider |
| `native/` | C implementation, checked signatures, layout assertions and descriptor inputs |
| `upstream/` | Pinned raylib header and [upstream license](upstream/LICENSE) |

`[project]` identifies this project and sets its version to `0.1.0`.
`[package]` gives consumers the identity `org.voidlang.raylib` and entry
`src/raylib.void`. The entry imports the declaration module, whose
`native "org.voidlang.raylib"` marker identifies the provider. The descriptor
binds that provider to `/src/raylib_native.void`. The application's
`[native-requirements.raylib]` selects provider version `6.0.0` and ABI 3.
The lock records the exact source and native artifacts for the target.

For a local wrapper edit, change `libs/raylib/src/raylib.void` in your
application. Run `voidc tidy`, `voidc check .` and `voidc run .` to update
the reviewed local lock, check the edit and review its native approval.
For changes to the physical declarations or C provider, work in a VOID
source checkout and rebuild and qualify the matching provider. See
`tools/raylib/README.md` in the VOID source checkout for maintenance steps.

## Create and distribute a library

1. Run `voidc new lib --name mylib` to create a library project. Put public
   functions in the file named by `package.entry`, and keep source under
   `src/`. Choose stable project and package IDs and update the version
   when you release changes.
2. Run `voidc check .` inside the library. From a separate application,
   run `voidc add mylib --dir ../mylib` and test an import of `pkg/mylib`.
   The command copies a sibling library into `libs/mylib`. Edit that copy
   or keep your changes in the original library and test in a fresh
   consumer. Review local dependency edits with `voidc tidy`.
3. Publish the source and its license in a Git repository you control.
   Give consumers the HTTPS clone URL and the full lowercase commit SHA.
   They can add the exact release with
   `voidc add mylib --git REPOSITORY_URL --rev FULL_COMMIT_SHA`.
   Use `--subdir DIRECTORY` when the package manifest is in a
   repository subdirectory. Publish another commit for an update.
   Keep the consumer's `void.lock` with its manifest.

For a self-contained C11 provider, see “Declarative C packages” in
`docs/NATIVE_LIBRARIES.md` in the VOID source checkout. Keep checked
native declarations beside your public VOID source. The recipe declares
fixed input files and provider metadata. It accepts no package scripts or
extra link libraries. This bundled raylib provider needs a separate
compiler resource because it links the official raylib release library.

## Limits

Use [the ownership guide](OWNERSHIP.md) when working with resources or
callbacks. Borrowed values depend on a live parent, and each owned value
has a matching release operation. Callback registrations must be closed
or detached to drain pending invocations safely. Use text interpolation
or the typed formatting functions for formatted messages. Arbitrary C
varargs are unavailable.

Native locks are target-specific. The Linux full-device sanitizer runs
report 256 bytes in Gallium graphics allocation, 408 bytes in the
raylib WAV music context and 64 bytes in sound-alias conversion. These
reports remain unsuppressed.
