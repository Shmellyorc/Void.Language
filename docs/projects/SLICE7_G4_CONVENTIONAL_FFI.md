# Slice 7 Gate 4 conventional C FFI

The conventional profile loads a project-authorized C provider and checked adapter on Linux x86-64 GNU and Windows x64. It uses the existing C descriptors, typed IR and Runtime bridges. The separate [verified Linux profile](SLICE7_G4_LINUX.md) keeps its `system none` closure and admission rules.

## Use it in a VOID application

1. Create an application with `voidc new my-app` or add a manifest with `voidc init`. Keep the vendor's shared library, a checked C adapter and the target's descriptor under the project's `native/` directory. Keep the C header and Windows import library available at adapter build time.
2. Declare the target in `void.project.toml`. Each value names a file directly inside `native/`. An application has one provider and adapter per declared target.

   ```toml
   [native.linux]
   descriptor = "example-linux.descriptor"
   library = "libexample.so.1"
   adapter = "libexample_void.so"

   [native.windows]
   descriptor = "example-windows.descriptor"
   library = "example.dll"
   adapter = "example_void.dll"
   ```

3. Write the C contract against the vendor header. This Linux descriptor binds a function declared as `int example_add(int, int);`. Use LF line endings. The thunk and fixture fields identify the adapter inputs and intended provider build, respectively. Supply the corresponding 64-digit lowercase SHA-256 values. The probe compares the embedded descriptor bytes.

   ```text
   void-c-primitive 1 1
   provider org.example.math
   target x86_64-unknown-linux-gnu sysv-x64
   source /example_ffi.void
   thunk <64 hex digits>
   fixture <64 hex digits>
   effects direct-sync nonblocking no-callbacks
   call c-default
   aliases c_int:i32 c_long:i64 c_size:u64 c_bool:bool
   bind add example_add c_int c_int c_int
   ```

   On Windows use `target x86_64-pc-windows-msvc msvc-x64` and `c_long:i32`. Revision 2 adds C records and byte spans. Revision 3 adds opaque resources with named acquire and terminal release operations. The complete revision 3 fixture includes record offsets, span offset and length operands, and an owner contract. `effects direct-sync blocking no-callbacks` admits synchronous blocking calls in the conventional profile. [Full descriptor](../../tests/ffi/conventional.descriptor.in).

4. Generate the adapter. The compiler verifies the descriptor and emits C11 static assertions for the header's function signatures and record layout. The host C compiler handles target calling conventions, including Windows x64 record returns. Compile the adapter against the provider. The commands below assume the project root, `include/example.h`, and the Clang/LLD toolchain already used by the VOID installation.

   ```bash
   voidc native-bind native/example-linux.descriptor example.h -o example_adapter.c
   clang -std=c11 -shared -fPIC -Iinclude example_adapter.c -Lnative -Wl,-l:libexample.so.1 \
       -Wl,-rpath,'$ORIGIN' -o native/libexample_void.so
   ```

   ```bat
   voidc native-bind native\example-windows.descriptor example.h -o example_adapter.c
   clang --target=x86_64-pc-windows-msvc -fuse-ld=lld -std=c11 -shared -Iinclude example_adapter.c example.lib -o native\example_void.dll
   ```

   Use the provider's SONAME on Linux and its DLL import name on Windows. When a Linux library needs sibling libraries, each importing image should carry the appropriate `$ORIGIN`-relative RUNPATH. Put transitively imported application DLLs beside the provider on Windows and install any OS/vendor prerequisites the library requires.

5. Import the synthesized VOID module using the descriptor's `source` field. The module is compiler-owned. A project file with the same identity is a collision.

   ```void
   use "/example_ffi.void" as math

   func main():
       print(math.add(20, 22))
   ```

6. Check, run and build. `check` validates the descriptor and VOID source offline. Pass `--allow-native` on `run`, `build` or `emit` to authorize the declared provider. When the application imports the binding, an AOT build embeds the authorized plan in the executable. Use `--no-opt` for normal code generation, or use the default release project profile for O2. Native graphs bypass the JIT disk cache.

   ```text
   voidc check --project .
   voidc run --project . --allow-native
   voidc build --project . --allow-native -o dist/app
   ```

7. Ship the executable and its sibling `native/` directory together. Copy the provider, adapter and their packaged transitive libraries there. On Windows give the output an `.exe` extension. The executable resolves the provider from its own location. The native payload uses ordinary OS dependency resolution. The descriptor, header, import library and VOID sources are build inputs. Build and packaging are separate steps.

## Loader and lifetime

- Windows opens each absolute path with `LoadLibraryExW` and the `LOAD_LIBRARY_SEARCH_DLL_LOAD_DIR | LOAD_LIBRARY_SEARCH_SYSTEM32` flags. It checks declared provider exports and resolves the adapter entry with `GetProcAddress`. The OS can reuse existing modules and apply redirection and other normal loader rules. The adapter's imports follow those rules. Restricted filesystem search does not establish exact transitive identity before initializers. The loader temporarily enables `SEM_FAILCRITICALERRORS` on the calling thread so Windows reports a bad image as a diagnostic. [Microsoft loader reference](https://learn.microsoft.com/en-us/windows/win32/api/libloaderapi/nf-libloaderapi-loadlibraryexw) · [Export lookup](https://learn.microsoft.com/en-us/windows/win32/api/libloaderapi/nf-libloaderapi-getprocaddress) · [Thread error mode](https://learn.microsoft.com/en-us/windows/win32/api/errhandlingapi/nf-errhandlingapi-setthreaderrormode).
- Linux opens each absolute path with `dlopen(RTLD_NOW | RTLD_LOCAL)` and looks up declared exports and the adapter entry with `dlsym`. Dependencies follow the process's normal ELF search and symbol binding rules, including RUNPATH, loader environment and already loaded objects. `RTLD_LOCAL` limits symbol publication. [Linux loader reference](https://man7.org/linux/man-pages/man3/dlopen.3.html).
- Native initialization may run before export checks, before the adapter probe and before VOID startup. The descriptor probe checks agreement with the adapter. Use trusted native artifacts and a trusted execution environment. Conventional admission provides no sandbox or binary digest attestation.
- The binding keeps the provider and adapter loaded across calls, compatible JIT generation leases and terminal resource release. The existing G3 bridge checks Runtime and owner-thread identity and releases an owned native token exactly once. A failed acquisition carries no ownership even if its scratch word contains an existing token. Buffers use bounded copy-in/copy-out. Failed writes leave VOID storage intact. Native addresses stay inside the transport.
- The synchronous application entry determines which VOID code can execute. Standard modules may declare unused async APIs. The reachable IR is validated independently and rejects async and worker execution. Calls, including declared blocking calls, run synchronously on the calling thread in NativeSafe state. The application profile supports C default calling conventions and has no native callbacks, variadics or raw pointer values.

## Qualification

Local qualification uses the public CLI and independently compiled libraries. [Process regression](../../tests/ffi/conventional_process.py) · [VOID application](../../tests/ffi/conventional.void.in) · [C provider](../../tests/ffi/conventional_fixture.c) · [Native dependency](../../tests/ffi/conventional_dependency.cpp) · [G3 rejection injection](../../tests/ffi/conventional_rejection.c).

The application verifies signed integer and mixed `f32`/`f64` calls, LP64 or LLP64 `long`, C bool and void results, and a 16-byte record passed and returned by value. It checks input, output and in/out spans, a negative native status with discarded output, and a rejected capacity. Each successful application process forwards `payload 雪`, prints it and records exactly one native dependency initializer. Its terminal release log is `-2, 23`, covering a native release error and automatic Runtime shutdown. The closed-handle and rejected-acquisition cases exit 1 and log only `42` and `31`, respectively. The latter injects a live non-owning pointer into the rejected result word.

The test installs the compiler, creates a separate project, rejects unauthorized execution, checks without starting a native initializer, compiles the generated C adapter and deliberately rejects an incompatible C header. It runs normal and O2 JIT, builds and links both AOT modes, then relocates the binaries with their native payload. The executable import table has no dependency on the fixture libraries. The source and compiler install are moved aside before the relocated application runs from an unrelated working directory. Negative cases cover missing and invalid library images, a missing dependency with a candidate in the working directory, a missing provider export and a mismatched adapter. The missing-export and mismatched-adapter cases record initialization before rejection.

| Local profile | Result | Evidence |
| --- | --- | --- |
| Linux x86-64 Release | 21/21 | [CTest](evidence/slice7-g4-conventional/linux/release-ctest.txt) · [Native process receipts](evidence/slice7-g4-conventional/linux/release-LastTest.txt) |
| Linux x86-64 Debug | 21/21 | [CTest](evidence/slice7-g4-conventional/linux/debug-ctest.txt) · [Native process receipts](evidence/slice7-g4-conventional/linux/debug-LastTest.txt) |
| Linux x86-64 fatal ASan/UBSan | 21/21 | [CTest](evidence/slice7-g4-conventional/linux/strict-ctest.txt) · [Native process receipts](evidence/slice7-g4-conventional/linux/strict-LastTest.txt) |
| Windows 10 x64 build 19045 Release | 17/17 | [CTest](evidence/slice7-g4-conventional/windows/Release-regression.txt) · [Native process receipts](evidence/slice7-g4-conventional/windows/Release-LastTest.txt) |
| Windows 10 x64 build 19045 Debug | 17/17 | [CTest](evidence/slice7-g4-conventional/windows/Debug-regression.txt) · [Native process receipts](evidence/slice7-g4-conventional/windows/Debug-LastTest.txt) |

Linux ran the focused CTest selection at `-j 4`. All three Linux profiles reran the G1–G3 tests and the verified G4-L JIT, IR and AOT cases. The strict run instrumented the compiler, Runtime and conventional fixture, with fatal ASan, UBSan and leak detection. Additional Linux Release smoke runs printed `42` using conventional revision 1 and revision 2 descriptors in JIT and relocated AOT. [Linux platform](evidence/slice7-g4-conventional/linux/platform.txt) · [Sanitizer settings](evidence/slice7-g4-conventional/linux/sanitizers.txt) · [ELF dependency table](evidence/slice7-g4-conventional/linux/provider-dynamic-table.txt) · [Revision smoke runs](evidence/slice7-g4-conventional/linux/revision-smoke.txt).

Windows ran both configurations concurrently with `-j 2` for each CTest process. The normal and O2 packages reported Windows error 193 for a corrupt image and 126 for the missing dependency. The provider imports a separate native DLL with CRT startup. The independently compiled `g4_conventional_sample` export reads the input record through RDX and writes the result through the hidden RCX pointer. Both VOID modes verified the returned fields. The compiler, linker, executables and DLLs used local `C:` paths. The VM share carried source packets and evidence. [Release PE imports and disassembly](evidence/slice7-g4-conventional/windows/Release-native-images.txt) · [Debug PE imports and disassembly](evidence/slice7-g4-conventional/windows/Debug-native-images.txt).

The initial Linux builds caught a missing C++ include and a changed verified-profile diagnostic. The early process tests caught a fixture entry path and name collision, the unused standard-library async declaration restriction, and a hidden export on the rejection wrapper. The repeated build caught a filesystem equivalence check against the generated module's virtual source path. The sanitizer build also encountered an intermediate object that predated the updated analysis function signature. Rebuilding that translation unit resolved the stale symbol.

Initial Windows attempts exposed the vendored source and SDK attestation requirements of a fresh isolated checkout and a stale CMake source-directory binding. The local replay used the already-attested checkout with hash-verified temporary overlays and independent build directories. An intermediate source packet missed the updated editor-probe call site. The later parallel run caught the fixture's MSVC CRT deprecation warning and the reparse path to Clang in the installed linker manifest. The replay used the retained physical Clang path and a fixture-only CRT compile define. Both profiles then timed out at the corrupt-image case because the OS displayed a modal dialog. The Debug CLI regression also recorded a transient access-denied error removing a temporary executable. Both final parallel profiles completed on their first attempt after the thread error-mode change. [Original attempts and failures](evidence/slice7-g4-conventional/original/) · [Bad Image dialog](evidence/slice7-g4-conventional/original/windows-bad-image-dialog.png).

The Linux reusable trees are `build/llvm23-release`, `build/llvm23-debug` and `build/llvm23-strict`. The retained application packages and their source archives are under `build/slice7-g4-conventional/*-final-artifacts`. Windows builds and packages are under `C:\Users\a\VoidS7-G4Conventional`. The VM overlay restored all 221 original paths or their prior absence and left 21,806,333,952 free bytes on `C:`. The index and HEAD match the opening snapshot. The layered audit verifies all 1,944 opening paths and records the 14 modified source files and seven added fixture files. [Windows restoration receipt](evidence/slice7-g4-conventional/windows/overlay.json) · [Source patch](evidence/slice7-g4-conventional/source.patch) · [Layered Git inventory](evidence/slice7-g4-conventional/inventory.txt).

## Requirements for a raylib application

The next raylib step needs a native package for each target, its headers and the Windows import library, plus a descriptor for the selected API. A normal adapter can call raylib and its backend dependencies using the same loader. The C wrapper can combine a window creation call with `IsWindowReady` and publish a window owner only on success. It must close the window on the graphics thread. Resource shutdown currently requires a release that can run on any shutdown thread. A general thread-affine shutdown contract is still needed before relying on implicit teardown of a graphics window, texture or GPU resource.

The existing synchronous blocking profile can run the window and draw/input loop on the application thread. The loop should perform creation, drawing, input, and explicit cleanup on that thread. Window creation and OS event processing remain a platform-specific qualification. By-value graphics identifiers and records can use checked C layouts or an opaque owner supplied by a wrapper when their lifetime needs enforcement.

VOID strings can be converted to UTF-8 bytes using `to_utf8()`. A C wrapper can take a checked input span, reject embedded NUL and append a terminator in its own storage. It can release that storage after a call that consumes the string immediately. The reviewed raylib implementation stores the title pointer in window state, so an `InitWindow` wrapper needs to keep the title with the window owner through close. Direct `const char*` marshalling is outside this descriptor profile. [raylib core implementation](https://github.com/raysan5/raylib/blob/master/src/rcore.c) · [GLFW thread-safety rules](https://www.glfw.org/docs/latest/intro_guide.html#thread_safety).
