# G71 Linux release ABI qualification

Source baseline `0ef6baeb130f918132dd98acbec156f6c9dc8c68` (G70). The
qualification started with an exact Git archive of that revision and
applied the Linux HTTP provider recipe change below. The build, test
install, images, and binary artifacts are private. G69 remains on hold.

## Ubuntu build inputs

The support floor is Ubuntu 24.04 x86-64. The build and replays used rootless
Podman with Ubuntu 24.04.5 userspace and the owner's existing Linux kernel.
The base image ID is
`d9b8c34d61a9b9a3c44cab883d7d2f2619d674966ab520186f799cf32e3e0377`
and its upstream image digest is
`sha256:78d4744ead1f5b19b04d0df558ab182a5300903900755e515b2ca4d10eb2044d`.
Ubuntu supplied glibc and development CRT `2.39-0ubuntu8.12`, GCC/G++
`13.3.0-6ubuntu2~24.04.1`, and libstdc++ runtime
`14.2.0-4ubuntu2~24.04.1`. The build records include the exact Ubuntu
package list and the hashes of its CRT objects and libstdc++ linker input.

The pinned LLVM archive SHA-256 is
`25d814fda50d5fb0084b42be09605d8b1595d49d5e2e03a3345d7e112ff6f9f3`.
The extracted Clang and LLD reported 23.1.4 and LLVM commit
`7a941dfb2877a0b8cf30323fefa85ad019e0b0f5`. LLVM reported Release and RTTI
disabled. Clang selected Ubuntu's GCC 13 headers, CRT and libraries.
The pinned HTTP and host providers were rebuilt from their verified source
archives in Ubuntu. The host recipe verified the approved libsodium backport
and built against the profile-matched HTTP zlib archive. The Release host
provider build identity is
`a3f575ee7fe26a85ea924e097bbc770a030ba111f69d9ec0ea62929e44ca66c3`.
Provider receipts contain the exact source pins and installed file hashes.

Previous [G70](G70_LOCAL_LAUNCH_READINESS.md) and
[G29](../release/G29_FIRST_RELEASE_READINESS.md) results traced Ubuntu loader
errors to binaries linked on the newer developer userspace. The G70 compiler
required `GLIBC_2.44` and `GLIBCXX_3.4.35`. Its packaged Star Harbor
required `GLIBC_2.42` and `GLIBCXX_3.4.35`.

The first G71 bundle ran the compiler, LSP, JIT and AOT on Ubuntu. Its ELF
audit then rejected an absolute HTTP provider build path in `libidn2`'s
RUNPATH. GNU libtool copied that directory from the installed
`libunistring.la` link metadata despite `--disable-rpath`. The Linux recipe
now removes that temporary metadata before linking and gives `libidn2` a
`$ORIGIN` search path for its sibling `libunistring.so.5`. It verifies the
actual linked ELF before writing the provider's checksum receipt. This
keeps the pinned source archives, shared-library ABI and verification
policy. The rejected archive and the ELF audit are retained separately
from the qualified files.

## Build and install commands

Use the pinned base image above. Install Ubuntu's `build-essential`,
`cmake`, `ninja-build`, `python3`, `perl`, `pkg-config`, `git`,
`libffi-dev`, `libedit-dev`, `libxml2-dev`, `libzstd-dev`,
`zlib1g-dev`, `libtinfo-dev`, `ca-certificates`, `curl`, `openssl`
and `xz-utils` in the container. The tests also use the recorded Python,
Xvfb and Mesa packages. The private evidence contains the full lists and
commands for both qualification images.

Put the exact source archive at `/work/src`. Verify and extract the pinned
LLVM archive at `/work/sdk`, provide its matching
`void-sdk-identity.json`, and link `/work/src/.toolchains/llvm-23.1.4`
to that SDK. Give each existing setup script its pinned source archives
under the corresponding `.toolchains/*/downloads` directory.

```sh
export PATH=/work/sdk/bin:/usr/bin:/bin
cd /work/src
python3 -B tools/setup_http_deps.py --profile Release --jobs 4
python3 -B tools/setup_host_deps.py --profile Release --jobs 4
python3 -B tools/verify_http_dependencies.py \
  .toolchains/http/prefix/linux-aws-lc-5.12.0/Release Release Linux
python3 -B tools/verify_host_deps.py --prefix .toolchains/host/prefix/Release \
  --profile Release --platform Linux --architecture x86_64
cmake -S /work/src -B /work/build-release -G Ninja \
  -DCMAKE_BUILD_TYPE=Release -DBUILD_TESTING=ON -DVOID_SANITIZERS=OFF \
  -DVOID_LLVM_RESEARCH=OFF -DCMAKE_C_COMPILER=/work/sdk/bin/clang \
  -DCMAKE_CXX_COMPILER=/work/sdk/bin/clang++ -DLLVM_DIR=/work/sdk/lib/cmake/llvm \
  -DVOID_CLANG=/work/sdk/bin/clang -DVOID_LINKER=/work/sdk/bin/ld.lld \
  -DCMAKE_EXE_LINKER_FLAGS=--ld-path=/work/sdk/bin/ld.lld \
  -DVOID_HTTP_DEPS_ROOT=/work/src/.toolchains/http/prefix/linux-aws-lc-5.12.0/Release \
  -DVOID_HOST_DEPS_ROOT=/work/src/.toolchains/host/prefix/Release
cmake --build /work/build-release --parallel 4 --target voidc voidls voidup \
  void_project_header void_project_manifest void_project_creation
for component in compiler tooling; do
  cmake --install /work/build-release --config Release \
    --component "$component" --prefix /work/install-release
done
```

The private bundle uses that installed layout. It adds the relocatable
VOIDUP engineering bootstrap built by `tools/build_voidup.py`, plus the
verified SDK's Clang, LLD and Clang 23 resource directory under `sdk/`.
As in the existing VOIDUP package producer, the two driver lines in
`bin/void-native.txt` point to relative SDK paths. The native resource
inventory continues to verify the original installed Runtime and provider
bytes. The private evidence contains a file checksum manifest, a symlink
inventory and the bundle archive checksum.

## Bundle identities

The Release compiler reports build identity
`9ff35fd27ad8a23cc483fd5681f97fe78555c298277e5ed63c68740476eee807`.
The archive inventory covers 586 regular files and four relative symlinks.

| Binary | SHA-256 |
| --- | --- |
| `bin/voidc` | `cc252126dd2be3fe2032b9f26c08de539f760d8347a2606ed28033e2d09d43a4` |
| `bin/voidls` | `f2e64e9a04bf3aba17a01f5f243654214c8e335f2e5edaaf674a2e521e81b07c` |
| `bin/voidup` | `16d46e38fb9d2b0a6f34c44f4da526a8659916f37a12280645e7a2428cf5c1ab` |
| `sdk/bin/clang` | `c336d48383874cffaab0dc4c5c359456d871f21380b746520dde5c7f4c125a67` |
| `sdk/bin/ld.lld` | `0f8d53acaf6ff4990506a26739c12fb90315fdcc0430ec4c305ca75115774d4b` |

The bundled compiler and VOIDUP use `$ORIGIN/../lib/void` to load the
pinned GNU shared libraries. The include and library search paths come
from the bundled driver resources and the Ubuntu development packages.
Ubuntu supplies the ELF loader, libc, libm, libstdc++, libgcc, and
`libz.so.1` where required. The bundle supplies libidn2 2.3.8 and
libunistring 1.5. The official raylib runtime directly needs
`libX11.so.6`, libc and libm. The graphics replay uses Ubuntu's GLX
and Mesa software renderer. The qualification archive records each
object's complete version needs, DT_NEEDED, interpreter, RUNPATH, and
resolved `ldd` closure.

The private bundle is `G71-linux-x86_64-ubuntu24.04.tar.xz`, SHA-256
`c1823aad9fa26ddb94eb1fac6acaa26a35bb54a17425e8b0e23ba97230809cd2`.

| Object | G70 maximum GLIBC / GLIBCXX | G71 maximum GLIBC / GLIBCXX |
| --- | --- | --- |
| `voidc` | 2.44 / 3.4.35 | 2.38 / 3.4.32 |
| `voidls` | 2.38 / 3.4.35 | 2.38 / 3.4.32 |
| `voidup` | — | 2.38 / 3.4.32 |
| Bundled Clang and LLD | — | 2.34 / 3.4.30 |
| Packaged Star Harbor | 2.42 / 3.4.35 | 2.38 / 3.4.32 |

These values are ELF version requirements. The declared support floor
comes from execution on Ubuntu 24.04, with its system runtime and CRT.

## Installed replay on Ubuntu

The archived bundle was extracted at a new path. All 586 file checksums
and four symlinks matched. The replay mounted the extracted prefix read
only at `/opt/void-g71` in a separate Ubuntu container. The source,
original build tree, original SDK location, and developer home were
absent. Startup, installed standard library and native resource
verification, LSP initialize and shutdown, JIT, and independent AOT
passed. Loader inspection resolved `libunistring.so.5` beside the installed
`libidn2.so.0`. The private VOIDUP bootstrap started and listed an empty
fixture root.

The corrected Ubuntu Release and fatal ASan/UBSan builds each passed
10/10 focused tests. They covered manifests, project creation, profile
selection, dependency explanation, local and smart HTTPS Git acquisition,
managed native resolution, and raylib binding parity. The strict profile
kept leak detection and fatal sanitizer errors enabled. Headless raylib
checks covered native approval, exact catalog restoration, and tamper
rejection. The known G68 device leaks remain documented and unsuppressed.

## Star Harbor on the Ubuntu floor

The archived compiler created a fresh application and used the normal
`voidc add raylib` route. The existing preparer verified the official
Linux raylib 6.0 archive, prepared the native shim with the bundled SDK,
and preserved the upstream license. The full 600-symbol probe compiled,
linked, and resolved the supported interfaces.

The G70 source and PNG were copied into the new project. The PNG SHA-256 is
`da2798351adcee0fab52f19b4b889a60e0a9e8bae40c01c7ab75ec02e3e4e029`.
The game loaded the PNG from disk, uploaded its 48×48 texture and closed
the texture and window on exit. Native approval was saved in a real
pseudoterminal and reused by the standalone build. JIT, AOT and the
packaged executable rendered in an Xvfb X11 session with Mesa llvmpipe.
The captures show the ship moving 148 pixels to the right after keyboard
input in each run. This qualifies software-rendered X11 in Ubuntu
userspace. Physical GPU and desktop compositor coverage comes from G70
on the owner host.

The program opens `assets/ship.png` relative to its working directory.
Launching JIT from the parent with `--project` reproduced the missing
asset error. The delivery used the G70 layout with `assets/` beside the
verified package, launched from the outer directory with `./engine/app`.
The packaged executable SHA-256 is
`6576ff3b20672f81c8d91841d23be98c36bc3d6b2db3ce9edd420f0dce946252`.
It ran from the copied delivery alone in a second Ubuntu container with
the recorded runtime packages. That container had no compiler, SDK or
development checkout and ran with networking disabled. The verified
package includes the raylib runtime, shim, and upstream notice. The sibling
sprite retains its separate delivery checksum.

## Decision and limits

**INTEGRATE** the Linux provider recipe correction and the private
Ubuntu x86-64 qualification. The bundle relies on Ubuntu's declared
system libraries and C/C++ development prerequisites for native linking.
The first raylib add still uses Python 3.12+ and HTTPS. The packaged game
uses the system runtime and display libraries. A plain `voidc build`
executable can reference its installed toolchain's native resources.
Use the verified application package for relocation.

This milestone qualifies the stated Ubuntu userspace. Other Linux
distributions, older userspaces, CPU targets, native editor execution
on Ubuntu, Windows, and macOS retain their existing qualification
boundaries. Production VOIDUP trust and release distribution remain
separate. The G70 public GitHub acquisition result was outside this
replay. Read-only checks confirmed that the owner's selected compiler,
server, and VS Code settings still match the G70 hashes.
