# G59 native package developer workflow

Production baseline `808d4dce8564cce439213363b2e5f53e238e0a21` (G54). G55–G58 remain separate experiments. The live `origin/main` matched the baseline during verification.

## Developer workflow

```sh
voidc new TestGame
cd TestGame
voidc package list
voidc package search raylib
voidc add raylib
```

Use the [headless pixel example](../NATIVE_LIBRARIES.md#bundled-raylib) in
`src/app.void`. Then run these commands from the project directory.

```sh
voidc check .
voidc tidy --check
voidc resolve --project . --locked
voidc run . --allow-native
voidc build . --allow-native --profile release
```

`add` bundles source and prepares the native catalog. `--allow-native`
authorizes execution or AOT compilation against the reviewed project lock.
VS Code uses the matching `voidls` and needs no catalog environment variable.
First preparation needs Python 3.12+, the configured native development tools
and access to the GitHub release. The Windows path uses the x64 Visual Studio
Native Tools environment.

## Source selection and reproducibility

- `verified-packages/raylib.package.cmake` names the GitHub origin, G54 base
  revision, source subdirectory and package version. The published base
  identifies where the package came from. The shipped snapshot includes the
  G59 parameter comments and preparation recipe. CMake computes its SHA-256
  over a path-sorted list of file SHA-256 values, with each line composed of
  the hex digest, two spaces, the relative path and a newline. Discovery and
  `SOURCE_ORIGIN.txt` show the base and snapshot separately. The compiler
  build identity incorporates the snapshot digest, and the existing project
  lock covers the installed bytes including the receipt. The generated
  receipt is outside the snapshot digest. Bundling resolves the known name
  even though the full VOID monorepo exceeds the D3 whole-snapshot limits.
- An explicit `voidc add mylib --git URL` fetches HEAD through D3's bounded,
  isolated HTTPS Git process. The manifest and lock record the verified
  commit. Repeating the command reuses the declared pin. The optional
  `--rev` and `--subdir` constrain selection. Git verifies the whole shallow
  snapshot before selecting the subtree.
- `voidc add mylib --dir DIR` reuses the installed editable-path behavior.
  `--path` is equivalent. `voidc package install mylib --store DIR …`
  applies the same resolver to an explicitly selected local store project,
  which retains its own lock, views, source snapshots and prepared artifacts.
  A consumer that copies local source from the store creates its own lock.
  Sharing a native catalog across unrelated projects has no automatic route.
- `voidc add raylib` can restore an entirely absent installed view using the
  existing manifest, lock and same compiler/toolchain. Publication verifies
  the complete restored lock before keeping the package. The Release and
  sanitizer replays restored exact descriptor, binary, notices and lock
  bytes. Target or toolchain changes may require a separately reviewed lock.
- General Git and local packages remain source-only on acquisition. The
  documented native import flow can keep reviewed descriptors, C sources,
  headers and notices in a package for explicit preparation and admission.
  Automatic native preparation has one shipped recipe in G59. No public
  GitHub package was published for this qualification. Direct URL and nested
  source acquisition used the existing smart-HTTPS/TLS Git fixture.

Cargo's Git dependency handling demonstrates resolving a branch at addition
and recording the commit. NuGet source mapping demonstrates binding a named
dependency to an identified source. CMake's archive hash check and vcpkg's
ABI hash demonstrate verified input identity and artifact reuse. G59 reuses
the VOID Git, lock and native catalog implementations for those roles.
[Cargo Git dependencies](https://doc.rust-lang.org/cargo/reference/specifying-dependencies.html#specifying-dependencies-from-git-repositories)
[NuGet package source mapping](https://learn.microsoft.com/en-us/nuget/consume-packages/package-source-mapping)
[CMake ExternalProject archive hashes](https://cmake.org/cmake/help/latest/module/ExternalProject.html)
[vcpkg ABI identity](https://learn.microsoft.com/en-us/vcpkg/reference/binarycaching#abi-hash)

## Native preparation and execution

The compiler launches the installed raylib recipe in isolated Python mode,
using absolute interpreter search entries. The recipe fetches a bounded
release asset from GitHub, checks its size and SHA-256, and reads only the
known regular library and Windows import-library members. The shipped
header and license match the release files after Windows newline
normalization. Adapter generation still runs through `native-bind`.
The shim uses the installed Clang/LLD 23.1.4, target ABI and existing
raylib C parity assertions. Its descriptor identity incorporates build
inputs, the recipe, the Clang binary and the VOID compiler build identity.
[raylib 6.0 upstream release](https://github.com/raysan5/raylib/releases/tag/6.0)

The existing package transaction publishes the catalog under
`libs/raylib/.void/native`. That directory falls outside the source
tree digest. The existing native portion of `void.lock` separately
verifies its descriptor, adapter, upstream runtime, link files and
notice identities. Default selection applies only to the known local
source dependency and exact provider tuple. Existing manual project
catalog selection remains available. Check, tidy and editor analysis
can inspect prepared declarations without authorizing execution.
Explicit URL acquisition does not run package scripts.

Run and build without consent returned status 2. Missing catalog,
descriptor modification, adapter modification and post-lock C source
edits failed closed. Native artifacts and lock hashes and timestamps
stayed unchanged through repeat add, JIT and independently linked AOT
in both profiles. Sanitizer metadata initially embedded scratch
filenames. Relative C source spellings made two independent strict
native builds byte-identical while retaining fatal instrumentation.
The Windows recipe also uses LLD's reproducible PE linking flag.

## Startup and reuse

Linux Release on Ryzen 7 1700X, pinned to logical CPU 7. Five alternating
blocks used identical source and separate processes. The first case is
a new ordinary Debug object cache. Raylib reports the existing
`Debug cache: unavailable` policy. Cold system page cache was not forced.
First output was a checked line from the resulting application.

| Command scope | Median wall | Range | First output |
| --- | ---: | ---: | ---: |
| `--version` process proxy | 5.83 ms | 5.20–6.61 ms | — |
| Ordinary Debug first | 30.57 ms | one run | 26.36 ms |
| Ordinary Debug repeat | 11.67 ms | 10.86–31.79 ms | 8.40 ms |
| Ordinary O2 JIT | 11.39 ms | 10.46–33.19 ms | 8.23 ms |
| Ordinary direct AOT | 3.52 ms | 2.66–4.01 ms | 2.17 ms |
| Raylib locked resolution | 69.14 ms | 64.68–97.83 ms | — |
| Raylib check | 1,562.99 ms | 1,539.37–1,998.07 ms | — |
| Raylib Debug first observed | 1,687.67 ms | one run | 1,655.88 ms |
| Raylib Debug repeat | 1,717.89 ms | 1,682.48–2,039.45 ms | 1,686.83 ms |
| Raylib O2 JIT | 1,718.68 ms | 1,713.20–2,167.79 ms | 1,688.64 ms |
| Raylib direct AOT | 17.53 ms | 16.57–29.55 ms | 16.27 ms |

A separate function-entry and return trace placed 1,504 ms in frontend
analysis. Source graph validation accounted for 1,185 ms and frontend
program construction for 205 ms. Two source/lock captures took about 30 ms
each, including about 19 ms each for native selection and verification.
IR lowering and JIT took 226 ms, including a 35 ms outer dynamic load.
The sampled entry took 0.72 ms. The broader intervals include the
narrower timings. Breakpoints perturb timing, so the timing table uses
separate uninstrumented processes. Kernel policy allowed counting
but rejected sampled hardware-cycle recording.

A fresh Linux add, including download and shim preparation, took 5.87 s
in the retained lifecycle run. A separate preparation trace measured
0.570 s for HTTPS, archive verification and extraction, 0.010 s for
`native-bind` and 3.745 s for Clang and LLD. The repeat add completed
without downloads or Clang calls. A Linux raylib window opened on the RX 6600 through X11,
rendered one frame and closed successfully in 2.20 s total. The frontend
validation above explains the warm launch delay. The evidence did not
identify an equivalently safe redundant frontend operation to remove
within G59.

## Qualification

- Linux Release passed the clean-room lifecycle, the real display case,
  all 13 focused package, HTTPS, native and Runtime controls, and a
  separate compiler-component installation smoke.
- Fatal Linux ASan/UBSan/LeakSanitizer passed the headless lifecycle
  including debug/release JIT, independently linked AOT, tamper negatives
  and restoration. Three relevant fatal TSan Runtime controls passed when
  replayed on the unchanged G54 Runtime build. The third-party raylib
  release binary was held at its original upstream identity.
- Native Windows 10 x64 build 19045 passed the headless raylib lifecycle
  with both Release and Debug builds of the compiler. LLVM, Clang and LLD
  were 23.1.4. Both builds exercised the Debug and Release application
  profiles through JIT and independently linked AOT. PE headers showed a
  1,048,576-byte reserve in the compiler, language server, native controls
  and generated executables. The Release compiler's first install took
  9.58 s and its first/repeat O2 JIT runs took 2.183/2.175 s. The Debug
  compiler figures were 10.84 s and 9.457/9.787 s. The focused CTest
  selection passed 8/8 in Debug and 7/8 in Release. The Release
  `project_maintenance` script reached interruption testing and could not
  launch the omitted `void_project_creation.exe` test helper. Debug built
  the helper and passed the interruption recovery test. Graphical
  execution was qualified on Linux.
- VS Code 1.141.0 executed the real extension and `voidls`. An open project
  transitioned from an unavailable package diagnostic to valid resolution
  after addition. It verified unsaved `rl.` completion, parameter hints,
  hover documentation, semantic function coloring, wrapper and native
  declaration definitions, invalid-call diagnostics and unsaved recovery.
- The established unsuppressed device findings remain recorded. Gallium
  retained 256 bytes, raylib's WAV music context retained 408 bytes and
  the sound-alias converter retained 64 bytes. The G59 sanitizer result
  is headless and the Linux display result is an ordinary Release run.

## Disposition

**INTEGRATE** the qualified bundled raylib workflow and source-package CLI
for independent review. Automatic preparation currently covers the one
reviewed native recipe. Explicit source registration remains project-scoped.
The measured warm JIT cost remains in source-graph validation.

The original qualification archive preserves the source patch, file SHA-256
values, Windows build identities, native and editor receipts, and replay
instructions. Integration preparation corrects the bundled provenance and
verifies it separately. The Windows qualification and the measured 1.69 s
warm startup remain results for the original qualified build.
