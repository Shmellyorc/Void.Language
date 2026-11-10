# Slice 7 real raylib application

The [VOID raylib gallery](../../examples/raylib/) opens a 640 × 360 native window. Left and Right move the orange box, the pink ball tracks the pointer, and the green ball advances every frame. Escape or the window’s close button ends the loop. The application owns one raylib texture and releases it before closing the window. The Linux desktop operator independently confirmed keyboard movement, pointer tracking, and animation.

## Qualification

Normal means `--no-opt`. Optimized uses O2 from the project’s release configuration. The compiler build profile is independent of the application optimization setting.

| Compiler and host | JIT normal | JIT O2 | Relocated AOT normal | Relocated AOT O2 | Affected regressions |
| --- | --- | --- | --- | --- | --- |
| Linux x64 Release | Pass | Pass | Pass | Pass | 21/21 |
| Linux x64 Debug | Pass | Pass | Pass | Pass | 21/21 |
| Linux x64 strict, fatal ASan/UBSan | Pass¹ | Pass¹ | Pass¹ | Pass¹ | 21/21 with leak detection |
| Windows 10 x64 Release | Pass | Pass | Pass | Pass | 17/17 |
| Windows 10 x64 Debug | Pass | Pass | Pass | Pass | 17/17 on retry |

¹ The graphics runs disabled LeakSanitizer after the first leak-enabled run exited on a 256-byte Gallium allocation. A separate sanitized native C executable dynamically loaded the same upstream raylib and reproduced the allocation on both AMD hardware and llvmpipe after a clean close. Two allocation stack PCs resolve to the Mesa Gallium library in the control’s pre-unload maps. ASan and UBSan stayed fatal for the VOID Runtime and adapter. The official Linux raylib binary itself was uninstrumented. The original leak-enabled graphics failure and both controls are retained. [Initial failure](evidence/slice7-raylib/failures/linux-graphics-leak-enabled.txt) · [Mesa address attribution](evidence/slice7-raylib/failures/linux-mesa-address-attribution.txt) · [Native C control](evidence/slice7-raylib/linux/driver-control.c)

Each passing mode measured real pixels before and after OS pointer and Right-key events, then sent the OS window manager’s ordinary close message. The harnesses verified motion of all three objects, successful process exit, and exactly one owner release marker. Linux checked both mapped libraries in `/proc/PID/maps`. Windows checked the two absolute DLL paths with Unicode PSAPI. The normal JIT and O2 AOT runs intentionally left the resource in the ledger for Runtime teardown. The other two modes exercised explicit VOID close.

| Evidence | Before input | After input | Results |
| --- | --- | --- | --- |
| Linux Release relocated AOT O2 | [Frame](evidence/slice7-raylib/linux/release/aot-optimized/frame-before.png) | [Frame](evidence/slice7-raylib/linux/release/aot-optimized/frame-after.png) | [Pixel positions and exit](evidence/slice7-raylib/linux/release/aot-optimized/result.json) |
| Windows 10 Release relocated AOT O2 | [Frame](evidence/slice7-raylib/windows/release/aot-optimized/frame-before.png) | [Frame](evidence/slice7-raylib/windows/release/aot-optimized/frame-after.png) | [Pixel positions, DLLs and exit](evidence/slice7-raylib/windows/release/aot-optimized/result.json) |

The focused matrix covered G1–G4, project CLI and manifests, and applicable GC, reload, analysis, and CLI-contract tests. The resource regression verifies that a foreign thread cannot acquire an owner-affine resource or close a Runtime with one live, and that the application owner can retry and release it once. It also rejects a raw import of the new Runtime bridge. Existing G3 call ownership, admission and terminal release cases ran in the same suites. [Linux logs and captures](evidence/slice7-raylib/linux/) · [Windows logs and captures](evidence/slice7-raylib/windows/)

## raylib 6.0 and runtime dependencies

The opening Linux inventory found no raylib shared library or headers in the system paths checked. The Windows inventory found synthetic VOID test libraries and no actual raylib installation among accessible paths. Its scan recorded 2,353 inaccessible paths. The approved official releases both contain shared libraries, headers, and static alternatives. The Windows package also contains the DLL import library `raylibdll.lib`. [Windows inventory](evidence/slice7-raylib/windows/initial-inventory.json) · [Archive contents and binary identities](evidence/slice7-raylib/vendor-artifacts.txt)

| Input | SHA-256 | Verification |
| --- | --- | --- |
| `raylib-6.0_linux_amd64.tar.gz` | `b64ba618a19e7da9e9c0e09bb398ecfd477a77d2d7231901bafc8739d27c08d2` | Matches upstream release asset |
| `raylib-6.0_win64_msvc16.zip` | `c93c7dc74576e00e3ee57fa2bd5fd109fbfc5aca87e12046dd7ec2c2268b3f78` | Matches upstream release asset |
| `raylib-6.0-source.tar.gz` | `2b3ee1e2120c7a0796b33062c7e9a694dd8a8caa56a96319ac8c8ecf54a90d0b` | Locally computed. No independently published SHA-256 established for this tag archive |

The original archives are retained under `build/slice7-raylib/downloads/`. [Upstream release asset hashes](https://github.com/raysan5/raylib/releases/expanded_assets/6.0) · [Exact tag archive URL](https://github.com/raysan5/raylib/archive/refs/tags/6.0.tar.gz) · [Local checksums](evidence/slice7-raylib/archive-sha256.txt)

- **Linux x64 SysV.** The qualified release shared library is `libraylib.so.6.0.0` packaged under its SONAME `libraylib.so.600`, SHA-256 `1041653dd5c1cb8c67494fa398a296520d3ffec20cf1bf71b1eaa4b96ac61aee`. It imports libc, libm and libX11. GLFW opens the GLX/GL driver at runtime. Qualification used a visible XWayland window on an AMD Radeon RX 6600 with Mesa 26.2.6 reporting OpenGL 4.6. The installed gallery also resolves the normal VOID and C++ runtime dependencies, including `libidn2.so.0` and `libunistring.so.5`, from the qualified host’s system directories. [Linux dependency resolution](evidence/slice7-raylib/linux/relocated-dependencies.txt)
- **Windows 10 x64 MSVC ABI.** The upstream GLFW DLL loaded, then failed window creation on build 19045. The guest exposes Microsoft GDI Generic WGL 1.1. A local official-source shared build with CMake `PLATFORM=Desktop`, `OPENGL_VERSION=1.1`, and `BUILD_SHARED_LIBS=ON` also failed the independent C window control. The working DLL uses upstream’s native `PLATFORM_DESKTOP_WIN32` and `GRAPHICS_API_OPENGL_SOFTWARE` configuration with the installed Clang/LLD 23.1.4 and MSVC x64 libraries. The DLL still imports OPENGL32 for compiled WGL helpers. GDI presents the software framebuffer. Other imports are GDI32, WINMM, USER32 and KERNEL32. The independent C control rendered 300 frames and closed before VOID testing. [Upstream Makefile](https://github.com/raysan5/raylib/blob/6.0/src/Makefile) · [Compatibility build recipe](evidence/slice7-raylib/windows/compat-software.py) · [Native C run](evidence/slice7-raylib/windows/software-visible.txt) · [DLL and import library hashes](evidence/slice7-raylib/windows/software-result.json)

The `raylib.h` used by both adapters had SHA-256 `047e7255f93f8c34039cab906ad76136706b5c7b4c5b5b065d84141963ee9b6b`. The working Windows DLL SHA-256 is `3bf4db7515f019512026f72baacb8e6c9f98f32786593571e98d28df9dafa824`, and its import library SHA-256 is `59778ec137ffebe87a6c099944330f20b7eecdf1d49a5a93de7000a8992339df`.

## Checked ABI and ownership

The project manifest points to a G4 conventional checked provider. The descriptor declares `direct-sync blocking no-callbacks` so the application thread runs graphics calls, including `EndDrawing`, which can wait for presentation and frame pacing and polls input. The generated adapter validates the transport and C layout. VOID passes `Color` by value through the generated thunk to a shim that calls the real raylib `ColorBrightness`. The C shim checks sizes, alignments and selected field offsets of `Color`, `Vector2`, `Image`, and `Texture2D` against the installed raylib headers. It keeps the window and texture behind one opaque resource.

The title starts as VOID UTF-8 bytes and crosses a checked input span capped at 255 bytes. The shim validates UTF-8, rejects an embedded NUL, and makes a terminated copy. A static buffer holds the selected title because raylib retains its address. The `--reject-title` case exited with status 2 before opening a window in JIT and relocated AOT. Initialization verifies both `IsWindowReady` and `IsTextureValid`. The shim unloads the temporary CPU image immediately. On normal release it unloads the texture before `CloseWindow`.

The generic addition is conventional descriptor policy `shutdown-owner` alongside the existing `shutdown-any`. The Runtime records the first committed application entry thread with a non-recycled identity. Typed IR selects the owner-aware bridge only for that descriptor policy. Acquisition, borrows and explicit close enforce affinity. With a live owner-affine resource, a foreign Runtime close fails before setting the closing flag or draining the ledger. The application owner can retry. During a race with an acquisition that has yet to publish, the existing native-job and publication rules leave rollback on the acquiring thread. An embedding host must keep the application thread available through shutdown. This milestone covers one desktop window per process and its associated texture.

[Descriptor template](../../examples/raylib/native/raylib.descriptor.in) · [C shim](../../examples/raylib/native/raylib_provider.c) · [VOID source](../../examples/raylib/src/gallery.void) · [Linux generated adapter](evidence/slice7-raylib/linux/raylib_checked.c) · [Windows generated adapter](evidence/slice7-raylib/windows/raylib_checked.c) · [Official raylib header](https://github.com/raysan5/raylib/blob/6.0/src/raylib.h) · [Official raylib core implementation](https://github.com/raysan5/raylib/blob/6.0/src/rcore.c) · [GLFW thread-safety rules](https://www.glfw.org/docs/3.4/intro_guide.html#thread_safety) · [MSVC x64 ABI](https://learn.microsoft.com/en-us/cpp/build/x64-calling-convention) · [Clang x86 ABI lowering](https://github.com/llvm/llvm-project/blob/main/clang/lib/CodeGen/Targets/X86.cpp)

## Build and run the sample

Copy `examples/raylib/` into a writable project directory. Use the Slice 7 compiler build, Python 3, the existing Clang/LLD, and local raylib 6.0 files. On Windows, use an x64 MSVC developer shell and the corresponding DLL import library. The guest used a private local `TEMP` and `TMP` directory under the user profile for AOT linking. Adjust the executable and file paths for the platform.

```sh
python3 prepare.py --voidc /path/to/voidc --clang /path/to/clang \
  --header /path/to/include/raylib.h \
  --library /path/to/lib/libraylib.so.6.0.0 \
  --license /path/to/LICENSE
voidc check --project .
voidc run --project . --allow-native --no-opt
voidc run --project . --allow-native
voidc build --project . --allow-native --no-opt -o /path/to/package/gallery
voidc build --project . --allow-native -o /path/to/other-package/gallery
```

On Windows, use `python`, `voidc.exe`, and an output name of `gallery.exe`. Pass `--library C:\path\to\raylib.dll --import-library C:\path\to\matching\raylib.lib` for the qualified software build, with `raylib.h` and `LICENSE` from the same source tree. `prepare.py` runs public `voidc native-bind`, compiles the generated C and the shim into a shared provider, and stages the separate raylib shared library. For the strict Linux compiler add `--sanitize` to instrument the adapter. Native project execution and linking require `--allow-native`.

For AOT distribution copy these files from the prepared project’s `native/` directory into a sibling `native/` directory beside the emitted executable.

| Linux | Windows |
| --- | --- |
| `libvoid_raylib.so` | `void_raylib.dll` |
| `libraylib.so.600` | `raylib.dll` |
| `RAYLIB-LICENSE.txt` | `RAYLIB-LICENSE.txt` |

Run the executable by absolute path from any working directory. Pass `--implicit-close` to exercise Runtime cleanup. Each qualified AOT package ran after the project and compiler were renamed away from their original paths. Each linked executable moved to a path containing spaces and `雪`. The harness supplied an unrelated working directory and a system-only PATH. Native loading found both libraries beside the relocated executable. Linux’s existing linker still emits a RUNPATH to the compiler’s original runtime directory. The recorded `ldd` results show system runtime resolution with that directory absent. On Windows the relocation audit hashed all four packaged files per run. End users need the applicable OS runtime and a supported display environment. [Linux relocation receipts](evidence/slice7-raylib/linux/relocated-dependencies.txt) · [Windows relocation receipts](evidence/slice7-raylib/windows/graphics-packages.json)

## Preserved failures and scope

- The Windows upstream GLFW DLL and the GLFW OpenGL 1.1 rebuild reported WGL pixel-format failure. An earlier CMake attempt used inconsistent Clang frontends and was rerun with matching clang-cl frontends. The first native Win32 software DLL link exposed WGL symbols and was rerun with the existing system OPENGL32 import library. The original and working backend records are separate. [Original failures](evidence/slice7-raylib/failures/) · [Working Windows build](evidence/slice7-raylib/windows/software-build.txt)
- The first Windows graphics attempt stopped when CMake installation encountered an absent TOML license in the guest’s thin qualification source. The rerun staged the freshly built compiler, generated install manifests, SHA-verified link archives and stdlib locally. The Windows compiler installer itself was outside this qualification. Later captures corrected framebuffer color quantization tolerance and switched from ToolHelp to PSAPI for Unicode module paths. A rejected default AOT temp root was replaced with a private local directory. [Initial installer failure](evidence/slice7-raylib/failures/graphics-first.txt) · [Corrective Windows graphics run](evidence/slice7-raylib/windows/graphics-psapi.txt)
- The first Linux XWayland pointer warp failed the pink centroid check. Targeted X11 motion events corrected the input test. A separate early dependency check matched the word “raylib” in the path to the sample; the correction checks only ELF `DT_NEEDED` entries. [Original Linux test receipts](evidence/slice7-raylib/failures/)
- The first Windows Debug parallel regression run hit `WinError 5` in CLI test scratch. The same parallel matrix passed on retry. [Original Debug regression](evidence/slice7-raylib/windows/Debug-parallel-first.txt) · [Retry](evidence/slice7-raylib/windows/Debug-regression.txt)

The tested graphics paths are Linux x64 X11/GLX on this XWayland host and Windows 10 x64 with raylib’s Win32 software backend. The sample qualifies desktop window, input, and texture ownership. The leak-enabled Linux GUI run remains failed as recorded above.

[Exact source patch](evidence/slice7-raylib/source.patch) · [Layered Git inventory](evidence/slice7-raylib/inventory.txt) · [Workspace cleanup and retained artifacts](evidence/slice7-raylib/scratch-cleanup.txt)
